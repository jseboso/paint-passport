import * as cdk from 'aws-cdk-lib';
import { Construct } from 'constructs';
import * as s3 from 'aws-cdk-lib/aws-s3';
import * as s3deploy from 'aws-cdk-lib/aws-s3-deployment';
import * as s3n from 'aws-cdk-lib/aws-s3-notifications';
import * as dynamodb from 'aws-cdk-lib/aws-dynamodb';
import * as lambda from 'aws-cdk-lib/aws-lambda';
import { NodejsFunction } from 'aws-cdk-lib/aws-lambda-nodejs';
import * as apigwv2 from 'aws-cdk-lib/aws-apigatewayv2';
import { HttpLambdaIntegration } from 'aws-cdk-lib/aws-apigatewayv2-integrations';
import * as path from 'path';
import * as fs from 'fs';

interface AppConfig {
  deviceKeys: Record<string, string>;
  webKey: string;
}

function loadConfig(): AppConfig {
  const configPath = path.join(__dirname, '..', 'config.json');
  if (!fs.existsSync(configPath)) {
    throw new Error(
      'Missing infra/config.json. Copy infra/config.example.json to infra/config.json ' +
      'and fill in real random secrets before running cdk synth/deploy. ' +
      'This file is gitignored on purpose - never commit real secrets.'
    );
  }
  return JSON.parse(fs.readFileSync(configPath, 'utf-8'));
}

export class PaintPassportStack extends cdk.Stack {
  constructor(scope: Construct, id: string, props?: cdk.StackProps) {
    super(scope, id, props);

    const config = loadConfig();

    // ───────────────────────────────────────────
    //  Storage
    // ───────────────────────────────────────────
    const mediaBucket = new s3.Bucket(this, 'MediaBucket', {
      removalPolicy: cdk.RemovalPolicy.RETAIN,
      cors: [
        {
          allowedMethods: [s3.HttpMethods.PUT, s3.HttpMethods.GET],
          allowedOrigins: ['*'],
          allowedHeaders: ['*'],
        },
      ],
    });

    // One row per saved painting a device has uploaded.
    const paintingsTable = new dynamodb.Table(this, 'PaintingsTable', {
      partitionKey: { name: 'deviceId', type: dynamodb.AttributeType.STRING },
      sortKey: { name: 'paintingId', type: dynamodb.AttributeType.STRING },
      billingMode: dynamodb.BillingMode.PAY_PER_REQUEST,
      removalPolicy: cdk.RemovalPolicy.RETAIN,
    });

    // One row per device: whatever image is currently waiting to be shown.
    const inboxTable = new dynamodb.Table(this, 'InboxTable', {
      partitionKey: { name: 'deviceId', type: dynamodb.AttributeType.STRING },
      billingMode: dynamodb.BillingMode.PAY_PER_REQUEST,
      removalPolicy: cdk.RemovalPolicy.RETAIN,
    });

    const commonEnv = {
      BUCKET_NAME: mediaBucket.bucketName,
      PAINTINGS_TABLE: paintingsTable.tableName,
      INBOX_TABLE: inboxTable.tableName,
      DEVICE_KEYS_JSON: JSON.stringify(config.deviceKeys),
      WEB_KEY: config.webKey,
    };

    const fnEntry = (name: string) => path.join(__dirname, '..', 'functions', name, 'index.ts');
    const nodeDefaults = {
      runtime: lambda.Runtime.NODEJS_20_X,
      environment: commonEnv,
      timeout: cdk.Duration.seconds(10),
      bundling: { minify: true, sourceMap: false },
    };

    // ───────────────────────────────────────────
    //  Path 1: device -> S3 -> browser gallery
    // ───────────────────────────────────────────
    const presignUpload = new NodejsFunction(this, 'PresignUploadFn', {
      entry: fnEntry('presign-upload'),
      ...nodeDefaults,
    });
    mediaBucket.grantPut(presignUpload);

    const onPaintingUploaded = new NodejsFunction(this, 'OnPaintingUploadedFn', {
      entry: fnEntry('on-painting-uploaded'),
      ...nodeDefaults,
    });
    paintingsTable.grantWriteData(onPaintingUploaded);
    mediaBucket.addEventNotification(
      s3.EventType.OBJECT_CREATED,
      new s3n.LambdaDestination(onPaintingUploaded),
      { prefix: 'paintings/' },
    );

    const listPaintings = new NodejsFunction(this, 'ListPaintingsFn', {
      entry: fnEntry('list-paintings'),
      ...nodeDefaults,
    });
    paintingsTable.grantReadData(listPaintings);
    mediaBucket.grantRead(listPaintings);

    // ───────────────────────────────────────────
    //  Path 2: browser -> S3 -> converted BMP -> device inbox
    // ───────────────────────────────────────────
    const presignInboxUpload = new NodejsFunction(this, 'PresignInboxUploadFn', {
      entry: fnEntry('presign-inbox-upload'),
      ...nodeDefaults,
    });
    mediaBucket.grantPut(presignInboxUpload);

    // Container image Lambda: Pillow resize/crop -> raw 24-bit BMP matching
    // firmware/esp32_client/media.cpp's BMP layout exactly.
    const convertInboxImage = new lambda.DockerImageFunction(this, 'ConvertInboxImageFn', {
      code: lambda.DockerImageCode.fromImageAsset(
        path.join(__dirname, '..', 'functions', 'convert-inbox-image'),
      ),
      environment: commonEnv,
      timeout: cdk.Duration.seconds(30),
      memorySize: 512,
    });
    mediaBucket.grantReadWrite(convertInboxImage);
    inboxTable.grantWriteData(convertInboxImage);
    mediaBucket.addEventNotification(
      s3.EventType.OBJECT_CREATED,
      new s3n.LambdaDestination(convertInboxImage),
      { prefix: 'inbox-original/' },
    );

    const pollInbox = new NodejsFunction(this, 'PollInboxFn', {
      entry: fnEntry('poll-inbox'),
      ...nodeDefaults,
    });
    inboxTable.grantReadData(pollInbox);
    mediaBucket.grantRead(pollInbox);

    const ackInbox = new NodejsFunction(this, 'AckInboxFn', {
      entry: fnEntry('ack-inbox'),
      ...nodeDefaults,
    });
    inboxTable.grantReadWriteData(ackInbox);
    mediaBucket.grantReadWrite(ackInbox);

    // ───────────────────────────────────────────
    //  API
    // ───────────────────────────────────────────
    const httpApi = new apigwv2.HttpApi(this, 'PaintPassportApi', {
      corsPreflight: {
        allowOrigins: ['*'],
        allowMethods: [apigwv2.CorsHttpMethod.GET, apigwv2.CorsHttpMethod.POST],
        allowHeaders: ['content-type', 'x-device-key', 'x-web-key'],
      },
    });

    httpApi.addRoutes({
      path: '/devices/{deviceId}/paintings/presign',
      methods: [apigwv2.HttpMethod.POST],
      integration: new HttpLambdaIntegration('PresignUploadInt', presignUpload),
    });
    httpApi.addRoutes({
      path: '/paintings',
      methods: [apigwv2.HttpMethod.GET],
      integration: new HttpLambdaIntegration('ListPaintingsInt', listPaintings),
    });
    httpApi.addRoutes({
      path: '/devices/{deviceId}/inbox/presign',
      methods: [apigwv2.HttpMethod.POST],
      integration: new HttpLambdaIntegration('PresignInboxInt', presignInboxUpload),
    });
    httpApi.addRoutes({
      path: '/devices/{deviceId}/inbox',
      methods: [apigwv2.HttpMethod.GET],
      integration: new HttpLambdaIntegration('PollInboxInt', pollInbox),
    });
    httpApi.addRoutes({
      path: '/devices/{deviceId}/inbox/ack',
      methods: [apigwv2.HttpMethod.POST],
      integration: new HttpLambdaIntegration('AckInboxInt', ackInbox),
    });

    // ───────────────────────────────────────────
    //  Static web gallery + upload page
    // ───────────────────────────────────────────
    const webBucket = new s3.Bucket(this, 'WebBucket', {
      websiteIndexDocument: 'index.html',
      publicReadAccess: true,
      blockPublicAccess: s3.BlockPublicAccess.BLOCK_ACLS,
      removalPolicy: cdk.RemovalPolicy.RETAIN,
    });

    new s3deploy.BucketDeployment(this, 'DeployWeb', {
      sources: [s3deploy.Source.asset(path.join(__dirname, '..', '..', 'web'))],
      destinationBucket: webBucket,
    });

    new cdk.CfnOutput(this, 'ApiUrl', { value: httpApi.apiEndpoint });
    new cdk.CfnOutput(this, 'WebUrl', { value: webBucket.bucketWebsiteUrl });
    new cdk.CfnOutput(this, 'MediaBucketName', { value: mediaBucket.bucketName });
  }
}

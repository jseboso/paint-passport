// GET /paintings?deviceId=optional
// Called by the web gallery. No auth - this endpoint is read-only and the
// URLs it returns are short-lived presigned GETs, not the raw bucket.

import { DynamoDBClient } from '@aws-sdk/client-dynamodb';
import { DynamoDBDocumentClient, QueryCommand, ScanCommand } from '@aws-sdk/lib-dynamodb';
import { S3Client, GetObjectCommand } from '@aws-sdk/client-s3';
import { getSignedUrl } from '@aws-sdk/s3-request-presigner';

const ddb = DynamoDBDocumentClient.from(new DynamoDBClient({}));
const s3 = new S3Client({});
const TABLE = process.env.PAINTINGS_TABLE!;
const BUCKET = process.env.BUCKET_NAME!;

export const handler = async (event: any) => {
  const deviceId = event.queryStringParameters?.deviceId;

  let items: any[] = [];
  if (deviceId) {
    const res = await ddb.send(new QueryCommand({
      TableName: TABLE,
      KeyConditionExpression: 'deviceId = :d',
      ExpressionAttributeValues: { ':d': deviceId },
      ScanIndexForward: false,
      Limit: 100,
    }));
    items = res.Items ?? [];
  } else {
    const res = await ddb.send(new ScanCommand({ TableName: TABLE, Limit: 200 }));
    items = (res.Items ?? []).sort((a, b) => (a.createdAt < b.createdAt ? 1 : -1));
  }

  const paintings = await Promise.all(items.map(async (item) => ({
    deviceId: item.deviceId,
    paintingId: item.paintingId,
    createdAt: item.createdAt,
    url: await getSignedUrl(
      s3,
      new GetObjectCommand({ Bucket: BUCKET, Key: item.s3Key }),
      { expiresIn: 3600 },
    ),
  })));

  return {
    statusCode: 200,
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ paintings }),
  };
};

#!/usr/bin/env node
import * as cdk from 'aws-cdk-lib';
import { PaintPassportStack } from '../lib/paint-passport-stack';

const app = new cdk.App();

new PaintPassportStack(app, 'PaintPassportStack', {
  env: {
    account: process.env.CDK_DEFAULT_ACCOUNT,
    region: process.env.CDK_DEFAULT_REGION || 'us-east-1',
  },
});

// GET /devices/{deviceId}/inbox
// Called by the ESP32 periodically (e.g. from wifiTick()) to check whether
// something new is waiting. Auth: header x-device-key.

import { DynamoDBClient } from '@aws-sdk/client-dynamodb';
import { DynamoDBDocumentClient, GetCommand } from '@aws-sdk/lib-dynamodb';
import { S3Client, GetObjectCommand } from '@aws-sdk/client-s3';
import { getSignedUrl } from '@aws-sdk/s3-request-presigner';

const ddb = DynamoDBDocumentClient.from(new DynamoDBClient({}));
const s3 = new S3Client({});
const TABLE = process.env.INBOX_TABLE!;
const BUCKET = process.env.BUCKET_NAME!;
const DEVICE_KEYS: Record<string, string> = JSON.parse(process.env.DEVICE_KEYS_JSON || '{}');

export const handler = async (event: any) => {
  const deviceId = event.pathParameters?.deviceId;
  if (!deviceId) {
    return { statusCode: 400, body: JSON.stringify({ error: 'missing deviceId' }) };
  }

  const providedKey = event.headers?.['x-device-key'] ?? event.headers?.['X-Device-Key'];
  if (!DEVICE_KEYS[deviceId] || providedKey !== DEVICE_KEYS[deviceId]) {
    return { statusCode: 401, body: JSON.stringify({ error: 'invalid device key' }) };
  }

  const res = await ddb.send(new GetCommand({ TableName: TABLE, Key: { deviceId } }));
  const item = res.Item;

  if (!item || item.status !== 'ready') {
    return {
      statusCode: 200,
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ available: false }),
    };
  }

  const url = await getSignedUrl(
    s3,
    new GetObjectCommand({ Bucket: BUCKET, Key: item.s3Key }),
    { expiresIn: 300 },
  );

  return {
    statusCode: 200,
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({
      available: true,
      uploadId: item.uploadId,
      url,
      width: item.width,
      height: item.height,
    }),
  };
};

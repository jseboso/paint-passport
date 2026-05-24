// POST /devices/{deviceId}/inbox/ack
// Called by the ESP32 after it has successfully downloaded and displayed the
// inbox image, so it doesn't get served again. Deletes the S3 objects and
// marks the DynamoDB row consumed. Auth: header x-device-key.

import { DynamoDBClient } from '@aws-sdk/client-dynamodb';
import { DynamoDBDocumentClient, GetCommand, UpdateCommand } from '@aws-sdk/lib-dynamodb';
import { S3Client, DeleteObjectCommand } from '@aws-sdk/client-s3';

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

  if (item?.s3Key) {
    await s3.send(new DeleteObjectCommand({ Bucket: BUCKET, Key: item.s3Key })).catch(() => {});
  }
  if (item?.originalKey) {
    await s3.send(new DeleteObjectCommand({ Bucket: BUCKET, Key: item.originalKey })).catch(() => {});
  }

  await ddb.send(new UpdateCommand({
    TableName: TABLE,
    Key: { deviceId },
    UpdateExpression: 'SET #s = :consumed',
    ExpressionAttributeNames: { '#s': 'status' },
    ExpressionAttributeValues: { ':consumed': 'consumed' },
  }));

  return { statusCode: 200, body: JSON.stringify({ ok: true }) };
};

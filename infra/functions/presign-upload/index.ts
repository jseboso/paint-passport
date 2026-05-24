// POST /devices/{deviceId}/paintings/presign
// Called by the ESP32 right before it wants to upload a finished painting.
// Auth: header x-device-key must match config.json's deviceKeys[deviceId].
// Returns a short-lived S3 presigned PUT URL; the device then does a plain
// HTTPS PUT of the BMP bytes straight to S3 (no AWS SDK/signing on-device).

import { S3Client, PutObjectCommand } from '@aws-sdk/client-s3';
import { getSignedUrl } from '@aws-sdk/s3-request-presigner';

const s3 = new S3Client({});
const BUCKET = process.env.BUCKET_NAME!;
const DEVICE_KEYS: Record<string, string> = JSON.parse(process.env.DEVICE_KEYS_JSON || '{}');

export const handler = async (event: any) => {
  const deviceId = event.pathParameters?.deviceId;
  if (!deviceId) {
    return { statusCode: 400, body: JSON.stringify({ error: 'missing deviceId' }) };
  }

  const providedKey = event.headers?.['x-device-key'] ?? event.headers?.['X-Device-Key'];
  const expectedKey = DEVICE_KEYS[deviceId];
  if (!expectedKey || providedKey !== expectedKey) {
    return { statusCode: 401, body: JSON.stringify({ error: 'invalid device key' }) };
  }

  const paintingId = `${Date.now()}`;
  const key = `paintings/${deviceId}/${paintingId}.bmp`;

  const uploadUrl = await getSignedUrl(
    s3,
    new PutObjectCommand({ Bucket: BUCKET, Key: key, ContentType: 'image/bmp' }),
    { expiresIn: 300 },
  );

  return {
    statusCode: 200,
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ uploadUrl, paintingId, key }),
  };
};

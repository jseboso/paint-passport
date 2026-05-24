// POST /devices/{deviceId}/inbox/presign?contentType=image/jpeg
// Called by the browser when someone wants to send a photo to a device.
// Auth: header x-web-key must match config.json's webKey.

import { S3Client, PutObjectCommand } from '@aws-sdk/client-s3';
import { getSignedUrl } from '@aws-sdk/s3-request-presigner';

const s3 = new S3Client({});
const BUCKET = process.env.BUCKET_NAME!;
const WEB_KEY = process.env.WEB_KEY!;

export const handler = async (event: any) => {
  const deviceId = event.pathParameters?.deviceId;
  if (!deviceId) {
    return { statusCode: 400, body: JSON.stringify({ error: 'missing deviceId' }) };
  }

  const providedKey = event.headers?.['x-web-key'] ?? event.headers?.['X-Web-Key'];
  if (!WEB_KEY || providedKey !== WEB_KEY) {
    return { statusCode: 401, body: JSON.stringify({ error: 'invalid web key' }) };
  }

  const contentType = event.queryStringParameters?.contentType || 'image/jpeg';
  const ext = contentType.includes('png') ? 'png' : 'jpg';
  const uploadId = `${Date.now()}`;
  const key = `inbox-original/${deviceId}/${uploadId}.${ext}`;

  const uploadUrl = await getSignedUrl(
    s3,
    new PutObjectCommand({ Bucket: BUCKET, Key: key, ContentType: contentType }),
    { expiresIn: 300 },
  );

  return {
    statusCode: 200,
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ uploadUrl, uploadId, key }),
  };
};

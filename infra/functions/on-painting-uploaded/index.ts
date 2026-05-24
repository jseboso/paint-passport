// S3 trigger on paintings/{deviceId}/{paintingId}.bmp being created.
// The device PUTs straight to S3 with no Lambda in that request path, so this
// is how we find out an upload actually landed and record it for the gallery.

import { DynamoDBClient } from '@aws-sdk/client-dynamodb';
import { DynamoDBDocumentClient, PutCommand } from '@aws-sdk/lib-dynamodb';

const ddb = DynamoDBDocumentClient.from(new DynamoDBClient({}));
const TABLE = process.env.PAINTINGS_TABLE!;

export const handler = async (event: any) => {
  for (const record of event.Records ?? []) {
    const key = decodeURIComponent(record.s3.object.key.replace(/\+/g, ' '));
    const parts = key.split('/'); // paintings/{deviceId}/{paintingId}.bmp
    if (parts.length !== 3 || parts[0] !== 'paintings') continue;

    const [, deviceId, filename] = parts;
    const paintingId = filename.replace(/\.bmp$/i, '');

    await ddb.send(new PutCommand({
      TableName: TABLE,
      Item: {
        deviceId,
        paintingId,
        s3Key: key,
        sizeBytes: record.s3.object.size,
        createdAt: new Date().toISOString(),
      },
    }));
  }

  return { statusCode: 200 };
};

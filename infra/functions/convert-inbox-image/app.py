"""
S3 trigger on inbox-original/{deviceId}/{uploadId}.{ext}.

Takes whatever photo the browser uploaded, fits/crops it to the device's
canvas aspect ratio, and writes a raw 24-bit BMP in the *exact* byte layout
firmware/esp32_client/media.cpp already reads (bottom-up, BGR, rows padded to
a 4-byte boundary) so the device can feed it straight into loadBMPToSprite()/
loadPainting() with no format changes on the firmware side.
"""

import io
import os
import time
import urllib.parse

import boto3
from PIL import Image

s3 = boto3.client("s3")
dynamodb = boto3.resource("dynamodb")

BUCKET = os.environ["BUCKET_NAME"]
INBOX_TABLE = os.environ["INBOX_TABLE"]

# Must match CANVAS_W / CANVAS_H in firmware/esp32_client/config.h
# (CANVAS_W = SCREEN_W = 480, CANVAS_H = SCREEN_H - TOOLBAR_H = 600)
CANVAS_W = 480
CANVAS_H = 600


def fit_and_crop(img: Image.Image, target_w: int, target_h: int) -> Image.Image:
    img = img.convert("RGB")
    src_w, src_h = img.size
    target_ratio = target_w / target_h
    src_ratio = src_w / src_h

    if src_ratio > target_ratio:
        new_w = int(src_h * target_ratio)
        left = (src_w - new_w) // 2
        img = img.crop((left, 0, left + new_w, src_h))
    else:
        new_h = int(src_w / target_ratio)
        top = (src_h - new_h) // 2
        img = img.crop((0, top, src_w, top + new_h))

    return img.resize((target_w, target_h), Image.LANCZOS)


def write_bmp24(img: Image.Image) -> bytes:
    img = img.convert("RGB")
    w, h = img.size
    row_size = ((w * 3 + 3) // 4) * 4
    pad = row_size - w * 3
    pixel_data_size = row_size * h
    file_size = 54 + pixel_data_size

    header = bytearray()
    header += b"BM"
    header += file_size.to_bytes(4, "little")
    header += (0).to_bytes(4, "little")          # reserved
    header += (54).to_bytes(4, "little")          # pixel data offset
    header += (40).to_bytes(4, "little")          # DIB header size
    header += w.to_bytes(4, "little")
    header += h.to_bytes(4, "little")
    header += (1).to_bytes(2, "little")           # planes
    header += (24).to_bytes(2, "little")          # bits per pixel
    header += (0).to_bytes(4, "little")           # compression
    header += pixel_data_size.to_bytes(4, "little")
    header += (2835).to_bytes(4, "little")         # x pixels/meter
    header += (2835).to_bytes(4, "little")         # y pixels/meter
    header += (0).to_bytes(4, "little")           # colors used
    header += (0).to_bytes(4, "little")           # important colors

    pixels = bytearray()
    px = img.load()
    for row in range(h - 1, -1, -1):  # BMP rows are stored bottom-up
        for col in range(w):
            r, g, b = px[col, row]
            pixels += bytes((b, g, r))
        pixels += bytes(pad)

    return bytes(header) + bytes(pixels)


def handler(event, context):
    for record in event.get("Records", []):
        bucket = record["s3"]["bucket"]["name"]
        key = urllib.parse.unquote_plus(record["s3"]["object"]["key"])

        parts = key.split("/")  # inbox-original/{deviceId}/{uploadId}.{ext}
        if len(parts) != 3 or parts[0] != "inbox-original":
            continue
        _, device_id, filename = parts
        upload_id = filename.rsplit(".", 1)[0]

        obj = s3.get_object(Bucket=bucket, Key=key)
        img = Image.open(io.BytesIO(obj["Body"].read()))
        img = fit_and_crop(img, CANVAS_W, CANVAS_H)
        bmp_bytes = write_bmp24(img)

        out_key = f"inbox/{device_id}/{upload_id}.bmp"
        s3.put_object(Bucket=BUCKET, Key=out_key, Body=bmp_bytes, ContentType="image/bmp")

        dynamodb.Table(INBOX_TABLE).put_item(Item={
            "deviceId": device_id,
            "uploadId": upload_id,
            "s3Key": out_key,
            "originalKey": key,
            "status": "ready",
            "width": CANVAS_W,
            "height": CANVAS_H,
            "createdAt": str(int(time.time())),
        })

    return {"statusCode": 200}

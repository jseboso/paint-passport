# Paint Passport - cloud backend

## 0. One-time prerequisites

- **Node.js 20+** - https://nodejs.org (LTS)
- **Docker Desktop** - required because `convert-inbox-image` deploys as a container
  image; `cdk deploy` builds it locally and needs a running Docker daemon.
- **An AWS account** with billing set up. If you don't have one yet: sign up at
  https://aws.amazon.com, then immediately go to **Billing → Budgets** and set a
  small budget alert (e.g. $5) so you get an email if anything runs away. At the
  traffic this project will see, actual cost should be a few cents a month at most.
- **An IAM user for deploying**, not your root account:
  1. AWS Console → IAM → Users → Create user (e.g. `paint-passport-deployer`).
  2. Attach policy `AdministratorAccess` for now - CDK needs broad permissions to
     create S3/Lambda/DynamoDB/IAM/API Gateway resources. Once things are stable you
     can tighten this to a scoped policy; for a personal project it's fine to relax
     later rather than fight IAM policies on day one.
  3. Security credentials tab → Create access key → "Command Line Interface (CLI)".
  4. Copy the Access Key ID and Secret Access Key somewhere safe - the secret is only
     shown once.
- **AWS CLI v2** - https://docs.aws.amazon.com/cli/latest/userguide/getting-started-install.html
  Then run:
  ```
  aws configure
  ```
  and paste in the access key, secret, a default region (e.g. `us-east-1`), and output
  format `json`.

## 1. Configure secrets (never committed)

```
cd infra
cp config.example.json config.json
```

Edit `config.json` and replace the placeholder strings with real random secrets - one
per device, plus one for the web upload form. Easy way to generate one:

```
openssl rand -hex 32
```

`config.json` is gitignored on purpose.

## 2. Install and deploy

```
cd infra
npm install
npx cdk bootstrap        # one-time per AWS account + region
npx cdk synth             # sanity check - renders the CloudFormation template, deploys nothing
npx cdk deploy
```

`cdk deploy` will print a diff of what it's about to create and ask to confirm (IAM
changes always require confirmation). It'll build the Docker image for the conversion
Lambda, push it to a CDK-managed ECR repo, and create everything else. Takes a few
minutes the first time.

At the end you'll see outputs like:

```
PaintPassportStack.ApiUrl = https://abc123xyz.execute-api.us-east-1.amazonaws.com
PaintPassportStack.WebUrl = http://paintpassportstack-webbucket....s3-website-us-east-1.amazonaws.com
PaintPassportStack.MediaBucketName = paintpassportstack-mediabucket-...
```

## 3. Point the web page at your API

```
cd ../web
cp config.example.js config.js   # if you haven't already
```

Edit `config.js` and set `apiUrl` to the `ApiUrl` output above. Then redeploy so the
updated site gets uploaded:

```
cd ../infra
npx cdk deploy
```

Open the `WebUrl` output in a browser. The gallery will say "No paintings yet" until a
device uploads something - that's expected.

## 4. Try it without any firmware changes yet

You can exercise the whole device-upload path with `curl` before touching the ESP32
code, using one of the device keys from `config.json`:

```
curl -X POST "$API_URL/devices/device-1/paintings/presign" \
  -H "x-device-key: <the device-1 secret from config.json>"
```

That returns `{"uploadUrl": "...", "paintingId": "...", "key": "..."}`. PUT any BMP
file to `uploadUrl` and refresh the gallery page - it should show up within a second or
two (the `on-painting-uploaded` Lambda fires off the S3 event).

## 5. Firmware side - what to add

Nothing above touches `firmware/`. When you're ready to wire the device in:

**Uploading a painting** (device → cloud): after `savePainting()` succeeds in
`media.cpp` and while `wifiIsConnected()`, POST to
`/devices/{deviceId}/paintings/presign` with header `x-device-key`, then do a plain
HTTPS PUT of the BMP bytes to the returned `uploadUrl` with `Content-Type: image/bmp`.
`WiFiClientSecure` can talk to both API Gateway and S3 with the standard Amazon Trust
Services root CA - no AWS SDK needed on-device, since the presigned URL carries the
signature.

**Receiving an image** (cloud → device): from `wifiTick()`, every so often (e.g. every
30–60s while idle) GET `/devices/{deviceId}/inbox` with header `x-device-key`. If
`available: true`, download the BMP from the returned `url` - it's already a raw
24-bit BMP at exactly 480×600, byte-for-byte compatible with `loadBMPToSprite()` in
`media.cpp`, so you can feed it into the same code path you already use for loading a
saved painting. After it's displayed, POST to `/devices/{deviceId}/inbox/ack` so it
doesn't get re-shown.

I didn't write this part yet since it touches your existing `.ino`/`wifi_setup.cpp`
state machine and I didn't want to guess at how you want it integrated with the
touch/UI flow - happy to draft it once the cloud side is deployed and you've confirmed
the endpoints work.

## 6. Cost and cleanup

Everything here is pay-per-use serverless (S3, Lambda, DynamoDB on-demand, API Gateway
HTTP API) - at personal-project traffic this is pennies a month, and DynamoDB/Lambda
both have permanent free tiers. To tear down:

```
cd infra
npx cdk destroy
```

Note: `MediaBucket`, `PaintingsTable`, and `InboxTable` are set to `RETAIN` on stack
deletion (so `cdk destroy` won't accidentally delete your paintings) - you'd need to
delete those manually from the console afterward if you actually want them gone.

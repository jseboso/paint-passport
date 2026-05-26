#pragma once

// Copy this file to cloud_secrets.h (gitignored) and fill in your real
// values. cloud_secrets.h is what cloud.cpp actually #includes - this file
// is just the checked-in template.
//
// CLOUD_API_BASE is the "ApiUrl" CloudFormation output from `cdk deploy`
// (see ../../CLOUD_README.md). No trailing slash.
//
// CLOUD_DEVICE_ID and CLOUD_DEVICE_KEY must exactly match one of the entries
// under "deviceKeys" in infra/config.json. Each physical device should have
// its own entry there, and therefore its own cloud_secrets.h at flash time -
// don't reuse the same device ID/key across two boards.

#define CLOUD_API_BASE   "https://REPLACE-ME.execute-api.us-east-1.amazonaws.com"
#define CLOUD_DEVICE_ID  "device-1"
#define CLOUD_DEVICE_KEY "1318797d2bdace61c3a4762c25f811c673cbc1588d72173cef8a67d98466967f"

// A note on cloud.cpp's use of WiFiClientSecure::setInsecure():
//
// Both the presign endpoint and the S3 URLs it hands back use TLS, but
// cloud.cpp skips certificate validation rather than pinning a root CA.
// That's the simplest option and a common tradeoff for personal IoT
// projects - it means a network-position attacker could in principle
// intercept traffic, but there's nothing sensitive flowing here beyond your
// own paintings and a shared secret you control. If you want to harden this
// later, pin the Amazon Root CA 1 certificate (what api-gateway/S3 chain to)
// instead of calling setInsecure().

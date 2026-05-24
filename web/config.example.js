// Copy this file to config.js (gitignored) and fill in the real values.
// config.js is what index.html actually loads.
window.APP_CONFIG = {
  // From the "ApiUrl" output after `cdk deploy` (infra/README section below).
  apiUrl: "https://REPLACE-ME.execute-api.us-east-1.amazonaws.com",

  // Not used by app.js directly (the web key is typed into the form each time),
  // kept here only as a reminder of what you set in infra/config.json.
  webKeyHint: "must match infra/config.json's \"webKey\"",
};

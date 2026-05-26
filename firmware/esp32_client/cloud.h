#pragma once
#include <stdint.h>
#include <stddef.h>

// Talks to the AWS backend described in ../../CLOUD_README.md:
//   - cloudUploadPainting(): device -> S3 -> web gallery
//   - cloudTick(): polls the device's cloud "inbox" for images sent from the
//     web page, and drops anything new straight into the SD media gallery.
//     It does NOT touch the live canvas, so it can never overwrite a
//     painting in progress - open a received image from the Media panel
//     whenever you want, same as any other saved painting.

void cloudInit();
void cloudTick();

// Uploads the BMP at localPath (e.g. "/paintings/003.bmp") to the cloud
// gallery. Blocking - the whole operation takes a moment, the same way an
// SD write itself does. Callers pass a path rather than a numeric index so
// this works for any file on SD, not just one whose gallery slot happens to
// line up with its filename's number (see media.cpp's uploadPaintingAtIndex,
// which is what most callers actually want).
bool cloudUploadPainting(const char* localPath);

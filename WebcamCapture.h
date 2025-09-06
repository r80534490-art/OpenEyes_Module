#pragma once

#ifdef WEBCAMLIBRARY_EXPORTS
#define WEBCAM_API __declspec(dllexport)
#else
#define WEBCAM_API __declspec(dllimport)
#endif

extern "C" {

	WEBCAM_API void __stdcall GetConnectedWebcams();
	WEBCAM_API void __stdcall CaptureWebcam();
}

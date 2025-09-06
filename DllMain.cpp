#include "pch.h"
// WebcamLibrary.cpp

#include "WebcamCapture.h"
#include <windows.h>
#include <dshow.h>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <comdef.h>

// Link against the DirectShow library
#pragma comment(lib, "strmiids.lib")

// Global log container and application data path.
std::vector<std::string> logs;
std::string appdata = ".";  // Using current directory for output

// Communication functions implemented with console I/O.
void send(const std::string& message) {
    std::cout << message << std::endl;
}

void sendAll(const std::string& message) {
    std::cout << message << std::endl;
}

std::string recv(int /*dummy*/) {
    std::string input;
    std::getline(std::cin, input);
    return input;
}

const int NO_BYTES_IN_BUFFER = -1;

// Function: GetConnectedWebcams
void GetConnectedWebcams()
{
    HRESULT hr;
    ICaptureGraphBuilder2* pBuild = nullptr;
    ICreateDevEnum* pDevEnum = nullptr;
    IPropertyBag* pPropBag = nullptr;
    IGraphBuilder* pGraph = nullptr;
    IEnumMoniker* pEnum = nullptr;
    IMoniker* pMoniker = nullptr;
    VARIANT var;
    VariantInit(&var);

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        send("COM initialization failed");
        return;
    }

    hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2, (void**)&pBuild);
    if (FAILED(hr))
    {
        send("Failed to create Capture Graph Builder");
        CoUninitialize();
        return;
    }

    hr = CoCreateInstance(CLSID_FilterGraph, 0, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&pGraph);
    if (FAILED(hr))
    {
        send("Failed to create Filter Graph");
        pBuild->Release();
        CoUninitialize();
        return;
    }

    hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));
    if (FAILED(hr))
    {
        send("Failed to create System Device Enumerator");
        pGraph->Release();
        pBuild->Release();
        CoUninitialize();
        return;
    }

    hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
    if (hr == S_FALSE)
    {
        send("NO_WEBCAMS");
        pDevEnum->Release();
        pGraph->Release();
        pBuild->Release();
        CoUninitialize();
        return;
    }

    std::wstring cams;
    // Enumerate webcams and build a list of friendly names.
    while (pEnum->Next(1, &pMoniker, NULL) == S_OK)
    {
        hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
        if (FAILED(hr))
        {
            pMoniker->Release();
            continue;
        }

        hr = pPropBag->Read(L"FriendlyName", &var, 0);
        if (SUCCEEDED(hr))
        {
            std::wstring camName = (wchar_t*)var.bstrVal;
            cams.append(camName + L"\n");
            VariantClear(&var);
        }

        pPropBag->Release();
        pMoniker->Release();
    }

    std::string cameraList(cams.begin(), cams.end());
    send(cameraList);

    // Clean up remaining COM objects
    pEnum->Release();
    pDevEnum->Release();
    pGraph->Release();
    pBuild->Release();
    CoUninitialize();
}

// Function: CaptureWebcam
void CaptureWebcam()
{
    HRESULT hr;
    ICaptureGraphBuilder2* pBuild = nullptr;
    IGraphBuilder* pGraph = nullptr;
    IBaseFilter* pCap = nullptr;
    IBaseFilter* pMux = nullptr;
    IMoniker* pMoniker = nullptr;
    IEnumMoniker* pEnum = nullptr;
    ICreateDevEnum* pDevEnum = nullptr;
    IMediaControl* pControl = nullptr;
    IPropertyBag* pPropBag = nullptr;
    VARIANT var;
    VariantInit(&var);

    // Output file path
    std::string filePath = appdata + "/wc.avi";

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        send("COM initialization failed");
        return;
    }

    hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2, (void**)&pBuild);
    if (FAILED(hr))
    {
        send("Failed to create Capture Graph Builder");
        CoUninitialize();
        return;
    }

    hr = CoCreateInstance(CLSID_FilterGraph, 0, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&pGraph);
    if (FAILED(hr))
    {
        send("Failed to create Filter Graph");
        pBuild->Release();
        CoUninitialize();
        return;
    }
    pBuild->SetFiltergraph(pGraph);

    hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));
    if (FAILED(hr))
    {
        send("Failed to create System Device Enumerator");
        pGraph->Release();
        pBuild->Release();
        CoUninitialize();
        return;
    }

    hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
    if (hr == S_FALSE)
    {
        send("NO_WEBCAMS");
        pDevEnum->Release();
        pGraph->Release();
        pBuild->Release();
        CoUninitialize();
        return;
    }

    // Automatically select the first available camera.
    hr = pEnum->Next(1, &pMoniker, NULL);
    if (FAILED(hr) || pMoniker == nullptr)
    {
        send("No valid camera found");
        pEnum->Release();
        pDevEnum->Release();
        pGraph->Release();
        pBuild->Release();
        CoUninitialize();
        return;
    }

    // Optionally, read and show the selected camera's friendly name.
    hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
    if (SUCCEEDED(hr))
    {
        hr = pPropBag->Read(L"FriendlyName", &var, 0);
        if (SUCCEEDED(hr))
        {
            std::wstring camName = (wchar_t*)var.bstrVal;
            std::string cameraName(camName.begin(), camName.end());
            send("Automatically selected camera: " + cameraName);
            VariantClear(&var);
        }
        pPropBag->Release();
    }

    // Read the capture duration from the user.
    send("Enter capture duration in milliseconds:");
    int duration = atoi(recv(0).data());
    if (duration <= 0)
    {
        send("No valid duration received");
        pMoniker->Release();
        pEnum->Release();
        pDevEnum->Release();
        pGraph->Release();
        pBuild->Release();
        CoUninitialize();
        return;
    }

    // Convert file path to wide string.
    std::wstring wideFilePath(filePath.begin(), filePath.end());

    // Bind the selected device to an IBaseFilter.
    hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&pCap);
    if (FAILED(hr))
    {
        send("Failed to bind camera to filter");
        pMoniker->Release();
        pEnum->Release();
        pDevEnum->Release();
        pGraph->Release();
        pBuild->Release();
        CoUninitialize();
        return;
    }

    // Add the capture filter to the graph.
    hr = pGraph->AddFilter(pCap, L"Capture Filter");
    if (FAILED(hr))
    {
        send("Failed to add capture filter to graph");
        pCap->Release();
        pMoniker->Release();
        pEnum->Release();
        pDevEnum->Release();
        pGraph->Release();
        pBuild->Release();
        CoUninitialize();
        return;
    }

    // Get the Media Control interface.
    hr = pGraph->QueryInterface(IID_IMediaControl, (void**)&pControl);
    if (FAILED(hr))
    {
        send("Failed to get media control interface");
        pCap->Release();
        pMoniker->Release();
        pEnum->Release();
        pDevEnum->Release();
        pGraph->Release();
        pBuild->Release();
        CoUninitialize();
        return;
    }

    // Set the output file name and add the file writer filter.
    hr = pBuild->SetOutputFileName(&MEDIASUBTYPE_Avi, wideFilePath.c_str(), &pMux, NULL);
    if (FAILED(hr))
    {
        send("Failed to set output file name");
        pControl->Release();
        pCap->Release();
        pMoniker->Release();
        pEnum->Release();
        pDevEnum->Release();
        pGraph->Release();
        pBuild->Release();
        CoUninitialize();
        return;
    }

    // Render the stream from the capture filter to the mux.
    hr = pBuild->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, pCap, NULL, pMux);
    if (FAILED(hr))
    {
        send("Failed to render stream");
        pControl->Release();
        pCap->Release();
        pMux->Release();
        pMoniker->Release();
        pEnum->Release();
        pDevEnum->Release();
        pGraph->Release();
        pBuild->Release();
        CoUninitialize();
        return;
    }

    // Start capturing.
    hr = pControl->Run();
    if (FAILED(hr))
    {
        send("Failed to run media control");
    }
    Sleep(duration);
    pControl->Stop();

    // Release COM interfaces.
    pBuild->Release();
    pGraph->Release();
    pCap->Release();
    pMux->Release();
    pMoniker->Release();
    pEnum->Release();
    pDevEnum->Release();
    pControl->Release();
    CoUninitialize();

    // Read the generated file and send its content.
    std::ifstream localFile(filePath, std::ios::binary);
    if (!localFile.is_open()) {
        send("invalid");
        return;
    }
    send("valid");

    try {
        std::vector<char> buf((std::istreambuf_iterator<char>(localFile)), std::istreambuf_iterator<char>());
        std::string contents(buf.begin(), buf.end());
        localFile.close();
        sendAll(contents);
        logs.push_back(filePath);
    }
    catch (const std::bad_alloc&) {
        sendAll("bad_alloc");
    }
}


// Standard DLL entry point
BOOL APIENTRY DllMain(HMODULE /*hModule*/,
    DWORD  ul_reason_for_call,
    LPVOID /*lpReserved*/)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:   break;
    case DLL_THREAD_ATTACH:    break;
    case DLL_THREAD_DETACH:    break;
    case DLL_PROCESS_DETACH:   break;
    }
    return TRUE;
}

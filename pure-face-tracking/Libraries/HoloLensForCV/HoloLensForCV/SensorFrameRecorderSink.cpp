//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "pch.h"

namespace HoloLensForCV
{
    SensorFrameRecorderSink::SensorFrameRecorderSink(
        _In_ Platform::String^ sensorName)
        : _sensorName(sensorName)
    {
    }

    SensorFrameRecorderSink::~SensorFrameRecorderSink()
    {
        Stop();
    }

    void SensorFrameRecorderSink::Start(
        _In_ Windows::Storage::StorageFolder^ archiveSourceFolder)
    {
        std::lock_guard<std::mutex> guard(
            _sinkMutex);

        REQUIRES(nullptr == _archiveSourceFolder);

        _archiveSourceFolder = archiveSourceFolder;
    }

    void SensorFrameRecorderSink::Stop()
    {
        std::lock_guard<std::mutex> guard(
            _sinkMutex);

        {
            wchar_t fileName[MAX_PATH] = {};

            swprintf_s(
                fileName,
                L"%s\\%s.csv",
                _archiveSourceFolder->Path->Data(),
                _sensorName->Data());

            CsvWriter csvWriter(
                fileName);

            {
                std::vector<std::wstring> columns;

                columns.push_back(L"Timestamp");
                columns.push_back(L"ImageFileName");

                columns.push_back(L"FrameToOrigin.m11"); columns.push_back(L"FrameToOrigin.m12"); columns.push_back(L"FrameToOrigin.m13"); columns.push_back(L"FrameToOrigin.m14");
                columns.push_back(L"FrameToOrigin.m21"); columns.push_back(L"FrameToOrigin.m22"); columns.push_back(L"FrameToOrigin.m23"); columns.push_back(L"FrameToOrigin.m24");
                columns.push_back(L"FrameToOrigin.m31"); columns.push_back(L"FrameToOrigin.m32"); columns.push_back(L"FrameToOrigin.m33"); columns.push_back(L"FrameToOrigin.m34");
                columns.push_back(L"FrameToOrigin.m41"); columns.push_back(L"FrameToOrigin.m42"); columns.push_back(L"FrameToOrigin.m43"); columns.push_back(L"FrameToOrigin.m44");

                csvWriter.WriteHeader(
                    columns);
            }

            for (const auto& recorderLogEntry : _recorderLog)
            {
                bool writeComma = false;

                csvWriter.WriteUInt64(
                    recorderLogEntry.Timestamp.UniversalTime,
                    &writeComma);

                csvWriter.WriteText(
                    recorderLogEntry.RelativeImagePath,
                    &writeComma);

                csvWriter.WriteFloat4x4(
                    recorderLogEntry.FrameToOrigin,
                    &writeComma);

                csvWriter.EndLine();
            }
        }

        _archiveSourceFolder = nullptr;
    }

    Platform::String^ SensorFrameRecorderSink::GetSensorName()
    {
        return _sensorName;
    }

    Windows::Media::Devices::Core::CameraIntrinsics^ SensorFrameRecorderSink::GetCameraIntrinsics()
    {
        return _cameraIntrinsics;
    }

    void SensorFrameRecorderSink::ReportArchiveSourceFiles(
        _Inout_ std::vector<std::wstring>& sourceFiles)
    {
        wchar_t csvFileName[MAX_PATH] = {};

        swprintf_s(
            csvFileName,
            L"%s.csv",
            _sensorName->Data());

        sourceFiles.push_back(
            csvFileName);

        for (const auto& recorderLogEntry : _recorderLog)
        {
            sourceFiles.push_back(
                recorderLogEntry.RelativeImagePath);
        }
    }

    void SensorFrameRecorderSink::Send(
        SensorFrame^ sensorFrame)
    {
        dbg::TimerGuard timerGuard(
            L"SensorFrameRecorderSink::Send: synchrounous I/O",
            20.0 /* minimum_time_elapsed_in_milliseconds */);

        std::lock_guard<std::mutex> lockGuard(
            _sinkMutex);

        if (nullptr == _archiveSourceFolder)
        {
            return;
        }

        //
        // Store a reference to the camera intrinsics.
        //
        if (nullptr == _cameraIntrinsics)
        {
            _cameraIntrinsics =
                sensorFrame->CameraIntrinsics;
        }

        //
        // Save the uncompressed image to disk. This is faster
        // than trying to compress images on the fly.
        //
        char absolutePath[MAX_PATH] = {};

        sprintf_s(
            absolutePath,
            "%S\\%020llu_%S.raw",
            _archiveSourceFolder->Path->Data(),
            sensorFrame->Timestamp.UniversalTime,
            _sensorName->Data());

#if DBG_ENABLE_VERBOSE_LOGGING
        dbg::trace(
            L"SensorFrameRecorderSink::Send: saving sensor frame to %S",
            absolutePath);
#endif /* DBG_ENABLE_VERBOSE_LOGGING */

        Windows::Graphics::Imaging::SoftwareBitmap^ softwareBitmap =
            sensorFrame->SoftwareBitmap;

        {
            FILE* file = nullptr;

            ASSERT(0 == fopen_s(&file, absolutePath, "wb"));

            Windows::Graphics::Imaging::BitmapBuffer^ bitmapBuffer =
                softwareBitmap->LockBuffer(
                    Windows::Graphics::Imaging::BitmapBufferAccessMode::Read);

            uint32_t pixelBufferDataLength = 0;

            uint8_t* pixelBufferData =
                Io::GetPointerToMemoryBuffer(
                    bitmapBuffer->CreateReference(),
                    pixelBufferDataLength);

            ASSERT(pixelBufferDataLength == fwrite(
                pixelBufferData,
                sizeof(uint8_t) /* _ElementSize */,
                pixelBufferDataLength /* _ElementCount */,
                file));

            ASSERT(0 == fclose(
                file));
        }

        {
            SensorFrameRecorderLogEntry recorderLogEntry;

            recorderLogEntry.Timestamp =
                sensorFrame->Timestamp;

            recorderLogEntry.FrameToOrigin =
                sensorFrame->FrameToOrigin;

            {
                wchar_t relativePath[MAX_PATH] = {};

                swprintf_s(
                    relativePath,
                    L"%020llu_%s.raw",
                    sensorFrame->Timestamp.UniversalTime,
                    _sensorName->Data());

                recorderLogEntry.RelativeImagePath =
                    std::wstring(relativePath);
            }

            _recorderLog.emplace_back(
                std::move(recorderLogEntry));
        }
    }
}

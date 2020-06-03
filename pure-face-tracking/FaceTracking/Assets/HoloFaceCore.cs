using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
using System;
using System.Linq;
using FaceProcessing;
using UnityEngine.Windows.Speech;

#if WINDOWS_UWP
using System.Threading.Tasks;
using Windows.Graphics.Imaging;
#endif

public class HoloFaceCore : MonoBehaviour
{
    public float LocalTrackerConfidenceThreshold = 900.0f;
    public int LocalTrackerNumberOfIters = 3;
    public int nLandmarks = 51;

    HololensCameraUWP webcam;
    LocalFaceTracker localFaceTracker; 
    FaceRenderer faceRenderer;    
    
    int nProcessing = 0;
    float imageProcessingStartTime = 0.0f;
    float elapsedTime = 0.0f;
    float lastTipTextUpdateTime = 0.0f;
    Queue<Action> executeOnMainThread = new Queue<Action>();

    KeywordRecognizer keywordRecognizer;  
    delegate void KeywordAction();
    Dictionary<string, KeywordAction> keywordCollection;

    bool showDebug = false;
    

    int frameReportPeriod = 10;
    int frameCounter = 0;

    void Start()
    {           
        webcam = GetComponent<HololensCameraUWP>();

        faceRenderer = GetComponent<FaceRenderer>();
        faceRenderer.SetWebcam(webcam);

        localFaceTracker = new LocalFaceTracker(LocalTrackerNumberOfIters, LocalTrackerConfidenceThreshold);

        if (PhraseRecognitionSystem.isSupported)
        {
            keywordCollection = new Dictionary<string, KeywordAction>();

            keywordCollection.Add("Show debug", ShowDebug);
            keywordCollection.Add("Hide debug", HideDebug);

            keywordRecognizer = new KeywordRecognizer(keywordCollection.Keys.ToArray());
            keywordRecognizer.OnPhraseRecognized += KeywordRecognizer_OnPhraseRecognized;
            keywordRecognizer.Start();
        }
    }

#if WINDOWS_UWP
    void Update ()
    {
        while (executeOnMainThread.Count > 0)
        {
            Action nextAction = executeOnMainThread.Dequeue();
            nextAction.Invoke();

            if (executeOnMainThread.Count == 0)
            {               
                imageProcessingStartTime = Time.realtimeSinceStartup;
                frameCounter++;
            }
        }

        if (nProcessing < 1)
        {
            SoftwareBitmap image = webcam.GetImage();
            if (image != null)
            {
                Matrix4x4 webcamToWorldTransform = webcam.WebcamToWorldMatrix;
                float[] landmarkProjections = faceRenderer.GetLandmarkProjections(webcamToWorldTransform);

                nProcessing++;
                Task.Run(() => ProcessFrame(image, landmarkProjections, webcamToWorldTransform));
            }
        }
    }

    private async Task ProcessFrame(SoftwareBitmap image, float[] landmarkInits, Matrix4x4 webcamToWorldTransform)
    {
        bool resetModelFitter = true;
        float[] landmarks = null;
        landmarks = await localFaceTracker.GetLandmarks(image, landmarkInits);
        resetModelFitter = localFaceTracker.ResetModelFitter;
        localFaceTracker.ResetModelFitter = false;

        if (landmarks != null)
        {
            executeOnMainThread.Enqueue(() => 
            {
                FrameProcessed(landmarks, webcamToWorldTransform, resetModelFitter);
            });
        }
        else
        {
            executeOnMainThread.Enqueue(() =>
            {
                faceRenderer.ResetFitter();
            });
        }

        nProcessing--;
    }
#endif

    private void FrameProcessed(float[] landmarks, Matrix4x4 webcamToWorldTransform, bool resetModelFitter)
    {
        if (resetModelFitter)
        {
            faceRenderer.ResetFitter();
        }
        faceRenderer.UpdateHeadPose(landmarks, webcamToWorldTransform);        
    }

    private void KeywordRecognizer_OnPhraseRecognized(PhraseRecognizedEventArgs args)
    {
        KeywordAction keywordAction;

        if (keywordCollection.TryGetValue(args.text, out keywordAction))
        {
            keywordAction.Invoke();
        }
    }

    void ShowDebug()
    {
        showDebug = true;
        faceRenderer.ShowDebug();       
    }

    void HideDebug()
    {
        showDebug = false;
        faceRenderer.HideDebug();        
    }
}

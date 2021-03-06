#ifndef MYTH_VIDEOOUT_NULL_H_
#define MYTH_VIDEOOUT_NULL_H_

// MythTV
#include "mythvideoout.h"

class MythVideoOutputNull : public MythVideoOutput
{
  public:
    static void GetRenderOptions(RenderOptions& Options);
    MythVideoOutputNull();
   ~MythVideoOutputNull() override;

    bool Init(const QSize& VideoDim, const QSize& VideoDispDim,
              float Aspect, MythDisplay* Display,
              const QRect& DisplayVisibleRect, MythCodecID CodecID) override;
    void SetDeinterlacing(bool Enable, bool DoubleRate, MythDeintType Force = DEINT_NONE) override;
    void PrepareFrame(VideoFrame* Frame, FrameScanType Scan, OSD* /*Osd*/) override;
    void Show(FrameScanType Scan) override;
    bool InputChanged(const QSize& VideoDim,   const QSize& VideoDispDim,
                      float        Aspect,     MythCodecID  CodecID,
                      bool&        AspectOnly, MythMultiLocker* Locks,
                      int          ReferenceFrames, bool ForceChange) override;
    void EmbedInWidget(const QRect& EmbedRect) override;
    void StopEmbedding(void) override;
    void UpdatePauseFrame(int64_t& DisplayTimecode, FrameScanType Scan = kScan_Progressive) override;
    void ProcessFrame(VideoFrame* Frame, OSD* Osd,
                      const PIPMap& PiPPlayers,
                      FrameScanType Scan) override;
    bool SetupVisualisation(AudioPlayer* /*Audio*/, MythRender* /*Render*/,
                            const QString& /*Name*/) override { return false; }


    void CreatePauseFrame(void);

  private:
    QMutex     m_globalLock   { QMutex::Recursive };
    VideoFrame m_avPauseFrame { };
};
#endif

#ifndef REPLAYER_H
#define REPLAYER_H
struct ReplayOptions
{
    ReplayOptions()
        :enableEscape(true), maxSleepTimeInMs(100), maxPollCnt(5),
        inplaceReplay(true), replayOptional(true), bar1MustMatch(true)
    {}
    bool enableEscape;
    unsigned int maxSleepTimeInMs;
    unsigned int maxPollCnt;
    bool inplaceReplay;
    bool replayOptional;
    bool bar1MustMatch;
};

namespace Replayer
{
    bool IsReplayingTrace();
}
#endif

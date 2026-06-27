#ifndef GeneratorAmQStrTdc_h
#define GeneratorAmQStrTdc_h

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

#include <fairmq/Device.h>

constexpr uint64_t Magic {0x4f464e492d4d4546};
constexpr int kOutBufByte {8};

struct FEMInfo {
    uint64_t magic    {Magic};
    uint32_t FEMId    {0};
    uint32_t FEMType  {0};
    uint64_t reserved {0};
};

class GeneratorAmQStrTdc : public fair::mq::Device
{
public:
    struct OptionKey {
        static constexpr std::string_view IpSiTCP           {"sitcp-ip"};
        static constexpr std::string_view OutputChannelName {"out-chan-name"};
    };

    GeneratorAmQStrTdc();
    GeneratorAmQStrTdc(const GeneratorAmQStrTdc&)            = delete;
    GeneratorAmQStrTdc& operator=(const GeneratorAmQStrTdc&) = delete;
    ~GeneratorAmQStrTdc() override = default;

protected:
    bool ConditionalRun() override;
    void Init() override;
    void InitTask() override;
    void PreRun() override;
    void PostRun() override;
    void ResetTask() override;

private:
    static uint64_t PackHeartbeatWord(uint32_t hbFrame, uint16_t hbFlag, uint16_t toffset);
    static uint64_t PackHeartbeat2ndWord(uint32_t transSize, uint32_t geneSize, uint16_t userReg);
    void SendFEMInfo();

    FEMInfo femInfo_;
    std::string ipSiTCP_ {"0"};
    std::string outputChannelName_;
    uint32_t tdcType_ {2};
    uint32_t hbFrame_ {0};
};

#endif

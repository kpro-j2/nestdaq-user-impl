#include <chrono>
#include <cstring>
#include <sstream>
#include <thread>

#include <fairmq/runDevice.h>

#include "AmQStrTdcData.h"
#include "GeneratorAmQStrTdc.h"

namespace bpo = boost::program_options;

namespace {
constexpr std::chrono::nanoseconds kHeartbeatPeriod {524288};
}

//______________________________________________________________________________
void addCustomOptions(bpo::options_description& options)
{
    using opt = GeneratorAmQStrTdc::OptionKey;
    options.add_options()
    (opt::IpSiTCP.data(), bpo::value<std::string>()->default_value("0"), "SiTCP IP (xxx.yyy.zzz.aaa)")
    (opt::OutputChannelName.data(), bpo::value<std::string>()->default_value("out"), "Name of the output channel");
}

//______________________________________________________________________________
std::unique_ptr<fair::mq::Device> getDevice(FairMQProgOptions&)
{
    return std::make_unique<GeneratorAmQStrTdc>();
}

//______________________________________________________________________________
GeneratorAmQStrTdc::GeneratorAmQStrTdc()
    : fair::mq::Device()
{
}

//______________________________________________________________________________
uint64_t GeneratorAmQStrTdc::PackHeartbeatWord(uint32_t hbFrame,
                                               uint16_t hbFlag,
                                               uint16_t toffset)
{
    constexpr uint64_t hbType = AmQStrTdc::Data::Heartbeat;
    return (hbType << 58)
         | (static_cast<uint64_t>(hbFlag) << 40)
         | (static_cast<uint64_t>(toffset) << 24)
         | (static_cast<uint64_t>(hbFrame) & 0x00ffffffu);
}

//______________________________________________________________________________
uint64_t GeneratorAmQStrTdc::PackHeartbeat2ndWord(uint32_t transSize,
                                                  uint32_t geneSize,
                                                  uint16_t userReg)
{
    constexpr uint64_t hbType = AmQStrTdc::Data::Heartbeat2nd;
    return (hbType << 58)
         | (static_cast<uint64_t>(userReg) << 40)
         | (static_cast<uint64_t>(geneSize & 0x000fffffu) << 20)
         | (static_cast<uint64_t>(transSize) & 0x000fffffu);
}

//______________________________________________________________________________
void GeneratorAmQStrTdc::SendFEMInfo()
{
    {
        uint64_t femId = 0;
        std::istringstream stream(ipSiTCP_);
        std::string token;
        int shift = 3;

        while (std::getline(stream, token, '.')) {
            if (shift < 0) {
                break;
            }
            uint32_t ipv = (std::stoul(token) & 0xff) << (8 * shift);
            femId |= ipv;
            --shift;
        }

        femInfo_.FEMId = static_cast<uint32_t>(femId);
        femInfo_.FEMType = tdcType_;
    }

    auto* fbuf = new uint8_t[sizeof(femInfo_)];
    uint8_t buf[8] = {0};

    for (int i = 0; i < 8; ++i) {
        buf[7 - i] = (femInfo_.magic >> (8 * (7 - i))) & 0xff;
    }
    std::memcpy(fbuf, &buf, sizeof(char) * 8);

    for (int i = 0; i < 8; ++i) {
        if (i < 4) {
            buf[7 - i] = (femInfo_.FEMId >> (8 * (3 - i))) & 0xff;
        } else {
            buf[7 - i] = (femInfo_.FEMType >> (8 * (7 - i))) & 0xff;
        }
    }
    std::memcpy(&fbuf[8], &buf, sizeof(char) * 8);

    uint8_t resv[8] = {0};
    std::memcpy(&fbuf[16], &resv, sizeof(char) * 8);

    FairMQMessagePtr initmsg(
        NewMessage(reinterpret_cast<char*>(fbuf),
                   kOutBufByte * 3,
                   [](void* object, void*) {
                       delete[] static_cast<uint8_t*>(object);
                   })
    );

    while (Send(initmsg, outputChannelName_) < 0) {
        LOG(warn) << "Fail to send FEMInfo";
    }
}

//______________________________________________________________________________
void GeneratorAmQStrTdc::Init()
{
}

//______________________________________________________________________________
void GeneratorAmQStrTdc::InitTask()
{
    using opt = OptionKey;

    ipSiTCP_ = fConfig->GetProperty<std::string>("msiTcpIp");
    outputChannelName_ = fConfig->GetProperty<std::string>(opt::OutputChannelName.data());
    tdcType_ = static_cast<uint32_t>(std::stoul(fConfig->GetProperty<std::string>("TdcType")));

    LOG(info) << "TPC IP: " << ipSiTCP_;
    LOG(info) << "TDC Type: " << tdcType_;
    LOG(info) << "Heartbeat period (ns): " << kHeartbeatPeriod.count();

    hbFrame_ = 0;
    SendFEMInfo();
}

//______________________________________________________________________________
void GeneratorAmQStrTdc::PreRun()
{
    LOG(info) << "Start generator";
}

//______________________________________________________________________________
void GeneratorAmQStrTdc::PostRun()
{
    LOG(info) << "End generator";
}

//______________________________________________________________________________
void GeneratorAmQStrTdc::ResetTask()
{
}

//______________________________________________________________________________
bool GeneratorAmQStrTdc::ConditionalRun()
{
    std::this_thread::sleep_for(kHeartbeatPeriod);

    auto* buffer = new uint8_t[kOutBufByte * 2] {};
    const uint64_t first = PackHeartbeatWord(hbFrame_ & 0x00ffffffu, 0, 0);
    const uint64_t second = PackHeartbeat2ndWord(0, 0, 0);

    std::memcpy(buffer, &first, sizeof(first));
    std::memcpy(buffer + sizeof(first), &second, sizeof(second));

    auto msg = NewMessage(reinterpret_cast<char*>(buffer), kOutBufByte * 2,
        [](void* object, void*) {
            delete[] static_cast<uint8_t*>(object);
        });

    ++hbFrame_;
    Send(msg, outputChannelName_);
    return true;
}

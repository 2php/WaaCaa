#include "Protocol.h"

#include "model/Chart.h"
#include "model/Dataset.h"
#include "model/ResponseUtils.h"
#include "window/WindowService.h"
#include "window/WindowInterface.h"
#include "utils/Log.h"

#include <string>

std::string Protocol::s_header = "_WCBEGIN";
std::string Protocol::s_tail   = "_WCEND";

bool Protocol::IsValidMessage(LinearBuffer &buffer)
{
    auto len = buffer.Length();
    auto buf = buffer.Buffer();

    if (len < 14) return false;

    auto fn = [](const std::string &gold, const char *bufferToCompare) -> bool {
        auto index = 0u;
        for (const char &g : gold) {
            if (bufferToCompare[index++] != g) return false;
        }
        return true;
    };

    if (!fn(s_header, buf + 0)) return false;
    if (!fn(s_tail, buf + len - s_tail.length())) return false;

    return true;
}

unsigned char Protocol::GetRequestMajorAndSubType(LinearBuffer &buffer)
{
    unsigned char majorType = buffer.Buffer()[s_header.length() + 10]; // 一共4种，所以占用2bit
    unsigned char subType   = buffer.Buffer()[s_header.length() + 11];
    return ((majorType << 6) | subType);
}

unsigned int Protocol::GetRequestBodyLength(LinearBuffer &buffer)
{
    const char *buf = buffer.Buffer();
    auto curr = s_header.length() + 1;

    unsigned int length = buf[curr++] << 3 * 8u;
    length += buf[curr++] << 2 * 8u;
    length += buf[curr++] << 1 * 8u;
    length += buf[curr++];

    return length;
}

void Protocol::ProcessRequestBody(LinearBuffer::Static *pReqBody, unsigned char reqMajorType, unsigned char reqSubType, LinearBuffer *pRespBuffer)
{
    if (pReqBody == nullptr || pRespBuffer == nullptr) {
        Log::e() << "wtf";
        return;
    }
    // assert(p_respBuffer != nullptr);
    char respTypeChar = ALL_RIGHT;


    if (RequestMajorType::A == (RequestMajorType)reqMajorType) {
        // Type A:
        if (RequestSubTypeA::CreateOneChart == (RequestSubTypeA)reqSubType) {
            auto chartIndex = WindowService::Instance().CreateOneWindow()->ChartIndex();
            if (chartIndex > 0) {
                respTypeChar = ALL_RIGHT;
                pRespBuffer->Append(&respTypeChar, 1);
                pRespBuffer->Append((const char *)(&chartIndex), 1);
            }
            else {
                respTypeChar = CREATE_CHART_FAILED;
                pRespBuffer->Append(&respTypeChar, 1);
            }
        }
        else if (RequestSubTypeA::CloseOneChart == (RequestSubTypeA)reqSubType) {
            unsigned char chartIndex = *(pReqBody->Buffer());
            if (WindowService::Instance().CloseWindow(chartIndex)) {
                respTypeChar = ALL_RIGHT;
            }
            else {
                respTypeChar = CHART_INDEX_NOT_FOUND;
            }
            pRespBuffer->Append(&respTypeChar, 1);
        }
        else if (RequestSubTypeA::CloseAllChart == (RequestSubTypeA)reqSubType) {
            WindowService::Instance().CloseAllWindow();
            respTypeChar = ALL_RIGHT;
            pRespBuffer->Append(&respTypeChar, 1);
        }
        else {
            respTypeChar = UNKNOWN_REQUEST;
            pRespBuffer->Append(&respTypeChar, 1);
        }
    }
    else if (RequestMajorType::B == (RequestMajorType)reqMajorType) {

    }
    else if (RequestMajorType::C == (RequestMajorType)reqMajorType) {
        // Type C:
        unsigned char dimType = reqSubType;

        unsigned char chartIndex = *(pReqBody->Buffer());
        unsigned char arrangeType = *(pReqBody->Buffer() + 1);
        unsigned char eleType = *(pReqBody->Buffer() + 2);

        const char *lenP = pReqBody->Buffer() + 3;

        // remember: please use "unsigned char" for temp var, or you need to mask it (e.g. &0xFF0000, &0xFF00, cause C/C++ use "int" by default)
        unsigned char tempByte;
        unsigned int nbBytesOfDataset = 0;
        tempByte = *(lenP++); nbBytesOfDataset += (tempByte << (3 * 8u));
        tempByte = *(lenP++); nbBytesOfDataset += (tempByte << (2 * 8u));
        tempByte = *(lenP++); nbBytesOfDataset += (tempByte << (1 * 8u));
        tempByte = *(lenP++); nbBytesOfDataset += tempByte;

        const void *dataset = (const void *)(lenP++);


        Chart *pChart = WindowService::Instance().FindByIndex(chartIndex);

        if (pChart != nullptr) {
            std::unique_ptr<Dataset> p(new Dataset((Dataset::Dimension)dimType, (Dataset::ArrangeType)arrangeType,(Dataset::ElemDataType)eleType, dataset, nbBytesOfDataset));
            p->GetId();

            // mvoe Dataset to pChart
            pChart->OnDataComming(p);
        }
    }
}

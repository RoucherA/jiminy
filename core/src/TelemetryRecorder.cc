///////////////////////////////////////////////////////////////////////////////
///
/// \brief TelemetryRecorder Implementation.
///
//////////////////////////////////////////////////////////////////////////////

#include <iomanip>
#include <fstream>

#include "exo_simu/core/TelemetryRecorder.h"


namespace exo_simu
{
    TelemetryRecorder::TelemetryRecorder(std::shared_ptr<TelemetryData const> const & telemetryDataInstance) :
    telemetryData_(telemetryDataInstance),
    flows_(),
    isInitialized_(false),
    recordedBytesLimits_(0),
    recordedBytesDataLine_(0),
    recordedBytes_(0),
    headerSize_(0),
    integersAddress_(),
    integerSectionSize_(0),
    floatsAddress_(),
    floatSectionSize_(0)
    {
        // Empty.
    }

    TelemetryRecorder::~TelemetryRecorder()
    {
        flows_.back().close();
    }

    result_t TelemetryRecorder::initialize(void)
    {
        result_t returnCode = result_t::SUCCESS;

        if (isInitialized_)
        {
            std::cout << "Error - TelemetryRecorder::initialize - TelemetryRecorder already initialized." << std::endl;
            returnCode = result_t::ERROR_INIT_FAILED;
        }

        std::vector<char_t> header;
        if (returnCode == result_t::SUCCESS)
        {
            // Reset the internal state
            reset();

            // Get telemetry data infos.
            telemetryData_->getData(integersAddress_,
                                    integerSectionSize_,
                                    floatsAddress_,
                                    floatSectionSize_);
            recordedBytesDataLine_ = integerSectionSize_ + floatSectionSize_ + static_cast<int64_t>(START_LINE_TOKEN.size() + sizeof(uint32_t));

            // Get the header
            telemetryData_->formatHeader(header);
            headerSize_ = header.size();

            // Create a new MemoryDevice and open it.
            returnCode = createNewChunk();
        }

        // Write the Header
        if (returnCode == result_t::SUCCESS)
        {
            returnCode = flows_[0].write(header);
        }

        if (returnCode == result_t::SUCCESS)
        {
            recordedBytes_ = headerSize_;
            isInitialized_ = true;
        }

        return returnCode;
    }

    void TelemetryRecorder::reset(void)
    {
        // Close the current MemoryDevice, if any and if it was opened.
        if (!flows_.empty())
        {
            flows_.back().close();
        }

        // Clear the MemoryDevice buffer
        flows_.clear();

        isInitialized_ = false;
    }

    result_t TelemetryRecorder::createNewChunk()
    {
        result_t returnCode = result_t::SUCCESS;

        // Close the current MemoryDevice, if any and if it was opened.
        if (!flows_.empty())
        {
            flows_.back().close();
        }

        // Create a new one.
        uint32_t isHeaderThere = flows_.empty();
        uint32_t maxRecordedDataLines = ((MAX_BUFFER_SIZE - isHeaderThere*headerSize_) / recordedBytesDataLine_);
        recordedBytesLimits_ = isHeaderThere*headerSize_ + maxRecordedDataLines * recordedBytesDataLine_;
        flows_.emplace_back(recordedBytesLimits_);
        returnCode = flows_.back().open(OpenMode::READ_WRITE);

        if (returnCode == result_t::SUCCESS)
        {
            recordedBytes_ = 0;
        }
        else
        {
            std::cout << "Error - TelemetryRecorder::createNewChunk - Impossible to create a new chunk of memory buffer." << std::endl;
            returnCode = result_t::ERROR_GENERIC;
        }

        return returnCode;
    }

    result_t TelemetryRecorder::flushDataSnapshot(float64_t const & timestamp)
    {
        result_t returnCode = result_t::SUCCESS;

        if (recordedBytes_ == recordedBytesLimits_)
        {
            returnCode = createNewChunk();
        }

        if (returnCode == result_t::SUCCESS)
        {
            // Write new line token
            flows_.back().write(START_LINE_TOKEN);

            // Write time
            flows_.back().write(static_cast<int32_t>(timestamp * 1e6));

            // Write data, integers first.
            flows_.back().write(reinterpret_cast<uint8_t const*>(integersAddress_), integerSectionSize_);

            // Write data, floats last.
            flows_.back().write(reinterpret_cast<uint8_t const*>(floatsAddress_), floatSectionSize_);

            // Update internal counter
            recordedBytes_ += recordedBytesDataLine_;
        }

        return returnCode;
    }

    void TelemetryRecorder::writeDataBinary(std::string const & filename)
    {
        std::ofstream myfile = std::ofstream(filename,
                                             std::ios::out |
                                             std::ios::binary |
                                             std::ofstream::trunc);

        for (uint32_t i=0; i<flows_.size(); i++)
        {
            int64_t pos_old = flows_[i].pos();
            flows_[i].seek(0);

            std::vector<uint8_t> bufferChunk;
            bufferChunk.resize(pos_old);
            flows_[i].readData(bufferChunk.data(), pos_old);
            myfile.write(reinterpret_cast<char*>(bufferChunk.data()), pos_old);

            if (i == flows_.size() - 1)
            {
                flows_[i].seek(pos_old);
            }
        }

        myfile.close();
    }

    void TelemetryRecorder::getData(std::vector<std::string>             & header,
                                    std::vector<float32_t>               & timestamps,
                                    std::vector<std::vector<int32_t> >   & intData,
                                    std::vector<std::vector<float32_t> > & floatData)
    {
        header.clear();
        timestamps.clear();
        intData.clear();
        floatData.clear();

        int32_t timestamp;
        std::vector<int32_t> intDataLine;
        intDataLine.resize(integerSectionSize_ / sizeof(int32_t));
        std::vector<float32_t> floatDataLine;
        floatDataLine.resize(floatSectionSize_ / sizeof(float32_t));

        for (uint32_t i=0; i<flows_.size(); i++)
        {
            int64_t pos_old = flows_[i].pos();
            flows_[i].seek(0);

            /* Dealing with version flag, constants, header, and descriptor.
               It makes the reasonable assumption that it does not overlap on several chunks. */
            if (i == 0)
            {
                flows_[i].seek(4); // Skip the version flag
                std::vector<char_t> headerCharBuffer;
                headerCharBuffer.resize(headerSize_ - 4);
                flows_[i].readData(headerCharBuffer.data(), headerSize_ - 4);
                char_t const * pHeader = &headerCharBuffer[0];
                uint32_t posHeader = 0;
                std::string fieldHeader(pHeader);
                while (true)
                {
                    header.emplace_back(std::move(fieldHeader));
                    posHeader += header.back().size() + 1;
                    fieldHeader = std::string(pHeader + posHeader);
                    if (fieldHeader.size() == 0 || posHeader >= headerCharBuffer.size())
                    {
                        break;
                    }
                    if (posHeader + fieldHeader.size() > headerCharBuffer.size())
                    {
                        fieldHeader = std::string(pHeader + posHeader, headerCharBuffer.size() - posHeader);
                        header.emplace_back(std::move(fieldHeader));
                        break;
                    }
                }
            }

            /* Dealing with data lines, starting with new line flag, time, integers, and ultimately floats. */
            uint32_t numberLines = (pos_old - flows_[i].pos()) / recordedBytesDataLine_;
            timestamps.reserve(timestamps.size() + numberLines);
            intData.reserve(intData.size() + numberLines);
            floatData.reserve(floatData.size() + numberLines);
            for (uint32_t j=0; j < numberLines; j++)
            {
                flows_[i].seek(flows_[i].pos() + START_LINE_TOKEN.size()); // Skip new line flag
                flows_[i].readData(reinterpret_cast<uint8_t *>(&timestamp), sizeof(int32_t));
                flows_[i].readData(reinterpret_cast<uint8_t *>(intDataLine.data()), integerSectionSize_);
                flows_[i].readData(reinterpret_cast<uint8_t *>(floatDataLine.data()), floatSectionSize_);
                timestamps.emplace_back(static_cast<float32_t>(timestamp) * 1e-6);
                intData.emplace_back(intDataLine);
                floatData.emplace_back(floatDataLine);
            }

            if (i == flows_.size() - 1)
            {
                flows_[i].seek(pos_old);
            }
        }
    }
}
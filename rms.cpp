//#include <stdio.h>
#include <unistd.h>
//#include <stdint.h>
//#include <string.h>
//#include <stdlib.h>
//#include <math.h>
#include <ctype.h>
#include <iostream>
#include <fstream>
#include <iomanip>

using namespace std;

#ifdef __cplusplus
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#endif

class Sample {
public:
	double rmsLeft;
	double rmsRight;
	double maxLeft;
	double maxRight;
	double samplesRead;

	Sample() {
		reset();
	}

	void reset() {
		rmsLeft = 0;
		rmsRight = 0;
		maxLeft = 0;
		maxRight = 0;
		samplesRead = 0;
	}

	void add(double l, double r) {
		rmsLeft += l * l;
		rmsRight += r * r;
		if (fabs(l) > maxLeft)
			maxLeft = fabs(l);
		if (fabs(r) > maxRight)
			maxRight = fabs(r);
		samplesRead++;
	}

	double getRmsLeft() {
		if (samplesRead == 0.0)
			samplesRead = 0.0000000001;
		return sqrt(rmsLeft / samplesRead);
	}

	double getRmsRight() {
		if (samplesRead == 0.0)
			samplesRead = 0.0000000001;
		return sqrt(rmsRight / samplesRead);
	}
};

double toDb(double amplitude) {
	if (amplitude == 0.0)
		amplitude += 0.0000000001;
	return 20. * log10(amplitude);
}

void formatTime(char* result, double duration) {
	int hours = duration / 3600;
	duration -= hours * 3600;

	int minutes = duration / 60;
	duration -= minutes * 60;

	int seconds = (int) duration;
	duration -= seconds;

	int millis = 1000 * duration;
	sprintf(result, "%02d:%02d:%02d.%04d", hours, minutes, seconds, millis);
}

void error(char const * errorMessage) {
	cerr << "\nERROR: " << errorMessage << "\n";
	exit(1);
}

// default step size in seconds
double resolution = 1;
bool verbose = false;

// output file 
char* outputFileName = NULL;
ofstream outputFile;

void avcodec_get_frame_defaults(AVFrame *frame){
#if LIBAVCODEC_VERSION_MAJOR >= 55
     // extended_data should explicitly be freed when needed, this code is unsafe currently
     // also this is not compatible to the <55 ABI/API
    if (frame->extended_data != frame->data && 0)
        av_freep(&frame->extended_data);
#endif

    memset(frame, 0, sizeof(AVFrame));
    av_frame_unref(frame);
}

// read data from data[0] and data[1]
void inline decodePlanarFrame(Sample *total, Sample *step, AVFrame *frame,
		AVCodecContext *codecContext, int channels, ssize_t size) {

	if ((codecContext->sample_fmt == AV_SAMPLE_FMT_U8)
			|| (codecContext->sample_fmt == AV_SAMPLE_FMT_U8P)) {
		if (channels == 2) {
			uint8_t *left = (uint8_t*) frame->data[0];
			uint8_t *right = (uint8_t*) frame->data[1];
			for (int i = 0; i < size; i++) {
				double l = (left[i] - 128) / 128.f;
				double r = (right[i] - 128) / 128.f;
				step->add(l, r);
				total->add(l, r);
			}
		} else if (channels == 1) {
			uint8_t *data = (uint8_t*) frame->data[0];
			for (int i = 0; i < size; i++) {
				double l = (data[i] - 128) / 128.f;
				double r = (data[i] - 128) / 128.f;
				step->add(l, r);
				total->add(l, r);
			}
		}
	} else if ((codecContext->sample_fmt == AV_SAMPLE_FMT_S16)
			|| (codecContext->sample_fmt == AV_SAMPLE_FMT_S16P)) {
		if (channels == 2) {
			int16_t *left = (int16_t*) frame->data[0];
			int16_t *right = (int16_t*) frame->data[1];
			for (int i = 0; i < size; i++) {
				double l = left[i] * (1.f / (1 << 15));
				double r = right[i] * (1.f / (1 << 15));
				step->add(l, r);
				total->add(l, r);
			}
		} else if (channels == 1) {
			int16_t *data = (int16_t*) frame->data[0];
			for (int i = 0; i < size; i++) {
				double l = data[i] * (1.f / (1 << 15));
				double r = data[i] * (1.f / (1 << 15));
				step->add(l, r);
				total->add(l, r);
			}
		}
	} else if ((codecContext->sample_fmt == AV_SAMPLE_FMT_S32)
			|| (codecContext->sample_fmt == AV_SAMPLE_FMT_S32P)) {
		if (channels == 2) {
			int32_t *left = (int32_t*) frame->data[0];
			int32_t *right = (int32_t*) frame->data[1];
			for (int i = 0; i < size; i++) {
				double l = left[i] * (1.f / (1U << 31));
				double r = right[i] * (1.f / (1U << 31));
				step->add(l, r);
				total->add(l, r);
			}
		} else if (channels == 1) {
			int32_t *data = (int32_t*) frame->data[0];
			for (int i = 0; i < size; i++) {
				double l = data[i] * (1.f / (1U << 31));
				double r = data[i] * (1.f / (1U << 31));
				step->add(l, r);
				total->add(l, r);
			}
		}
	} else if ((codecContext->sample_fmt == AV_SAMPLE_FMT_FLT)
			|| (codecContext->sample_fmt == AV_SAMPLE_FMT_FLTP)) {
		if (channels == 2) {
			float *left = (float*) frame->data[0];
			float *right = (float*) frame->data[1];
			for (int i = 0; i < size; i++) {
				double l = left[i];
				double r = right[i];
				step->add(l, r);
				total->add(l, r);
			}
		} else if (channels == 1) {
			float *data = (float*) frame->data[0];
			for (int i = 0; i < size; i++) {
				double l = data[i];
				double r = data[i];
				step->add(l, r);
				total->add(l, r);
			}
		}
	} else if ((codecContext->sample_fmt == AV_SAMPLE_FMT_DBL)
			|| (codecContext->sample_fmt == AV_SAMPLE_FMT_DBLP)) {
		if (channels == 2) {
			double *left = (double*) frame->data[0];
			double *right = (double*) frame->data[1];
			for (int i = 0; i < size; i++) {
				double l = left[i];
				double r = right[i];
				step->add(l, r);
				total->add(l, r);
			}
		} else if (channels == 1) {
			double *data = (double*) frame->data[0];
			for (int i = 0; i < size; i++) {
				double l = data[i];
				double r = data[i];
				step->add(l, r);
				total->add(l, r);
			}
		}
	} else {
		error("unsupported sample format");
	}
}

// read data from data[0] left/right interleaved
void inline decodeNonPlanarFrame(Sample *total, Sample *step, AVFrame *frame,
		AVCodecContext *codecContext, int channels, ssize_t size) {

	if ((codecContext->sample_fmt == AV_SAMPLE_FMT_U8)
			|| (codecContext->sample_fmt == AV_SAMPLE_FMT_U8P)) {
		if (channels == 2) {
			uint8_t *data = (uint8_t*) frame->data[0];
			for (int i = 0; i < channels * size; i += channels) {
				double l = (data[i] - 128) / 128.f;
				double r = (data[i] - 128) / 128.f;
				step->add(l, r);
				total->add(l, r);
			}
		} else if (channels == 1) {
			uint8_t *data = (uint8_t*) frame->data[0];
			for (int i = 0; i < size; i++) {
				double l = (data[i] - 128) / 128.f;
				double r = (data[i] - 128) / 128.f;
				step->add(l, r);
				total->add(l, r);
			}
		}
	} else if ((codecContext->sample_fmt == AV_SAMPLE_FMT_S16)
			|| (codecContext->sample_fmt == AV_SAMPLE_FMT_S16P)) {
		if (channels == 2) {
			int16_t *data = (int16_t*) frame->data[0];
			for (int i = 0; i < channels * size; i += channels) {
				double l = data[i] * (1.f / (1 << 15));
				double r = data[i + 1] * (1.f / (1 << 15));
				step->add(l, r);
				total->add(l, r);
			}
		} else if (channels == 1) {
			int16_t *data = (int16_t*) frame->data[0];
			for (int i = 0; i < size; i++) {
				double l = data[i] * (1.f / (1 << 15));
				double r = data[i] * (1.f / (1 << 15));
				step->add(l, r);
				total->add(l, r);
			}
		}
	} else if ((codecContext->sample_fmt == AV_SAMPLE_FMT_S32)
			|| (codecContext->sample_fmt == AV_SAMPLE_FMT_S32P)) {
		if (channels == 2) {
			int32_t *data = (int32_t*) frame->data[0];
			for (int i = 0; i < channels * size; i += channels) {
				double l = data[i] * (1.f / (1u << 31));
				double r = data[i + 1] * (1.f / (1u << 31));
				step->add(l, r);
				total->add(l, r);
			}
		} else if (channels == 1) {
			int32_t *data = (int32_t*) frame->data[0];
			for (int i = 0; i < size; i++) {
				double l = data[i] * (1.f / (1u << 31));
				double r = data[i] * (1.f / (1u << 31));
				step->add(l, r);
				total->add(l, r);
			}
		}
	} else if ((codecContext->sample_fmt == AV_SAMPLE_FMT_FLT)
			|| (codecContext->sample_fmt == AV_SAMPLE_FMT_FLTP)) {
		if (channels == 2) {
			float *data = (float*) frame->data[0];
			for (int i = 0; i < channels * size; i += channels) {
				double l = data[i];
				double r = data[i + 1];
				step->add(l, r);
				total->add(l, r);
			}
		} else if (channels == 1) {
			float *data = (float*) frame->data[0];
			for (int i = 0; i < size; i++) {
				double l = data[i];
				double r = data[i];
				step->add(l, r);
				total->add(l, r);
			}
		}
	} else if ((codecContext->sample_fmt == AV_SAMPLE_FMT_DBL)
			|| (codecContext->sample_fmt == AV_SAMPLE_FMT_DBLP)) {
		if (channels == 2) {
			double *data = (double*) frame->data[0];
			for (int i = 0; i < channels * size; i += channels) {
				double l = data[i];
				double r = data[i + 1];
				step->add(l, r);
				total->add(l, r);
			}
		} else if (channels == 1) {
			double *data = (double*) frame->data[0];
			for (int i = 0; i < size; i++) {
				double l = data[i];
				double r = data[i];
				step->add(l, r);
				total->add(l, r);
			}
		}
	} else {
		error("unsupported sample format");
	}
}

void printLine(Sample *step, double seconds) {
	if (outputFileName != NULL) {
		outputFile << fixed << seconds << " " << toDb(step->getRmsLeft()) << " "
				<< toDb(step->getRmsRight()) << " " << toDb(step->maxLeft)
				<< " " << toDb(step->maxRight) << "\n";
	} else {
		cout << fixed << seconds << " " << toDb(step->getRmsLeft()) << " "
				<< toDb(step->getRmsRight()) << " " << toDb(step->maxLeft)
				<< " " << toDb(step->maxRight) << "\n";
	}
}

void readAudioRms(const char* filename) {
	av_register_all();

	// overall RMS
	Sample* step = new Sample();
	Sample* total = new Sample();

	if (outputFileName != NULL) {
		outputFile.open(outputFileName);
		outputFile.precision(2);
	} else {
		cout.precision(2);
	}

	// --- open the file and autodetect the format
	AVFormatContext *formatContext = NULL;
	if (avformat_open_input(&formatContext, filename, NULL, NULL) < 0) {
		cerr << "cannot open input file '" << filename << "'\n";
		exit(1);
	}

	if (avformat_find_stream_info(formatContext, NULL) < 0)
		error("cannot find stream information");

    AVCodec *codec = NULL;

	// get stream id
	int audioStreamId = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
	if (audioStreamId < 0) error("cannot find an audio stream in the input file");

	AVCodecContext *codecContext = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context( codecContext, formatContext->streams[audioStreamId]->codecpar );

	if (avcodec_open2(codecContext, codec, NULL) < 0)
		error("cannot open audio decoder");

	int channels = codecContext->channels;
	cout << "channels=" << channels << "\n";
	if (channels < 1)
		error("invalid number of channels");

	int sampleRate = codecContext->sample_rate;
	cout << "sampleRate=" << sampleRate << "\n";
	if (sampleRate < 1)
		error("invalid sample rate");


	enum AVSampleFormat sampleFormat = codecContext->sample_fmt;
	bool isPlanar = av_sample_fmt_is_planar(sampleFormat);

	int sampleSize = av_get_bytes_per_sample(sampleFormat);

	if (verbose) {
		cout << "resolution=" << resolution << "\n";
		cout << "isPlanar=" << isPlanar << "\n";
		cout << "audioStreamid=" << audioStreamId << "\n";
		cout << "format=" << av_get_sample_fmt_name(sampleFormat) << "("
				<< sampleFormat << ")\n";
		cout << "sampleSize=" << sampleSize << "\n";
	}

	if (sampleSize < 1)
		error("invalid sample size");

	// write header
	if (outputFileName != NULL) {
		outputFile << "#seconds rmsL rmsR maxL maxR\n";
	} else {
		cout << "#seconds rmsL rmsR maxL maxR\n";
	}

	double seconds = 0;
	int steps = 0;
	unsigned long samplesReadPerSecond = 0;
	
	AVPacket packet;
	while (av_read_frame(formatContext, &packet) == 0) {
	
		if (packet.stream_index != audioStreamId) {
    		av_packet_unref(&packet);
			continue;
		}

    	AVFrame  frame; 
		avcodec_get_frame_defaults(&frame);

		int ret = avcodec_send_packet(codecContext, &packet);
    		av_packet_unref(&packet);
		
        if (ret < 0) {
            cout << "Error submitting the packet to the decoder\n";
            exit(1);
        }
        
        while (ret >= 0) {
            ret = avcodec_receive_frame(codecContext, &frame);

            if (ret == AVERROR(EAGAIN)){
                // did not receive an answer yet
                break;
            }else if ( ret == AVERROR_EOF){
                cout << "EOF\n";
                break;
            }else if (ret < 0) {
                cout << "Error during decoding\n";
                exit(1);
            }
            int data_size = av_get_bytes_per_sample(codecContext->sample_fmt);
            if (data_size < 0) {
                cout << "Failed to calculate data size\n";
                exit(1);
            }

		    int bufferSize = av_samples_get_buffer_size(NULL,
				    codecContext->channels, frame.nb_samples,
				    codecContext->sample_fmt, 1);
		    if (bufferSize < 0) {
			    cerr << "warn: invalid buffer size\n";
		    }

		    ssize_t size = bufferSize / (sampleSize * channels);

		    if (size == 0) continue;
		    

		    if (isPlanar == true) {
			    //like mp3
			    decodePlanarFrame(total, step, &frame, codecContext, channels,
					    size);
		    } else {
			    //like wav
			    decodeNonPlanarFrame(total, step, &frame, codecContext, channels,
					    size);
		    }
            
		    if (step->samplesRead > resolution * sampleRate) {
			    // print initial line
			    if (steps == 0)
				    printLine(step, steps);

			    seconds += step->samplesRead / sampleRate;
			    printLine(step, seconds);
			    step->reset();
			    steps++;
		    }

        }
	}

    avcodec_free_context(&codecContext);
    //av_frame_free(&frame);
    //av_packet_free(&packet);
    
	// add line for last value

	seconds += step->samplesRead / sampleRate;
	printLine(step, seconds);
	step->reset();
	steps++;

	if (total->samplesRead < 1)
		error("decoded audio data is empty");

	double duration = double(total->samplesRead) / double(sampleRate);

	char time[25];
	formatTime(time, duration);

	if (outputFileName == NULL) {
		cout << "#end of data\n\n";
	}

	if (verbose) {
		cout << "steps=" << steps << "\n";
		cout << "samples=" << (unsigned long) total->samplesRead << "\n";
	}
	cout << "duration=" << duration << "\n";
	cout << "duration=" << time << "\n";
	cout << "rmsLeft=" << toDb(total->getRmsLeft()) << "\n";
	cout << "rmsRight=" << toDb(total->getRmsRight()) << "\n";

	if (codecContext)
		avcodec_close(codecContext);
	avformat_close_input(&formatContext);
	if (outputFile.is_open() )
		outputFile.close();
		
    delete step;
    delete total;

}

void usage() {
	cout << "rms: calculate audio file rms values\n";
	cout << "Usage:rms -i <inputFile> [-o <outputFile>] [-r <resolution>]\n";
	cout << " -i inputFile     audio file to be read, uses ffmpeg\n";
	cout << " -o outputFile    write rms statistics\n";
	cout << " -r VALUE         RMS resolution for output lines in seconds\n";
	cout << " -v               verbose\n";
	cout << " -h               this help\n";
	exit(0);
}

int main(int argc, char *argv[]) {
	if (argc < 1) {
		error("missing arguments");
	}

	// set defaults
	resolution = 1;
	verbose = false;

	const char* inputFileName = NULL;
	int c;
	while ((c = getopt(argc, argv, "r:i:o:hv")) != -1) {
		switch (c) {
		case 'r':
			resolution = atof(optarg);
			break;
		case 'i':
			inputFileName = optarg;
			break;
		case 'o':
			outputFileName = optarg;
			break;
		case 'v':
			verbose = true;
			break;
		case 'h':
			usage();
			break;
		case '?':
			if (optopt == 'r')
				error("Option -r requires an argument");
			else if (optopt == 'o')
				error("Option -o requires an argument.\n");
			else {
				cerr << "Unknown option " << optopt;
				exit(1);
			}
			return 1;
		default:
			abort();
			break;
		}
	}

	if (inputFileName == NULL)
		error("missing -i <inputFileName>");

	cout << "inputFile='" << inputFileName << "'\n";
	if (outputFileName != NULL) {
		cout << "outputFile='" << outputFileName << "'\n";
		if (strncmp(inputFileName, outputFileName, 200) == 0) {
			error("output file name must be different from input file name");
		}
	}

	if (inputFileName != NULL)
		readAudioRms(inputFileName);
}


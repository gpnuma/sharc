/*
 * Centaurean Sharc
 * http://www.centaurean.com/sharc
 *
 * Copyright (c) 2013, Guillaume Voirin
 * All rights reserved.
 *
 * This software is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation. For the terms of this
 * license, see http://www.gnu.org/licenses/gpl.html
 *
 * You are free to use this software under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * Alternatively, you can license this library under a commercial
 * license, see http://www.centaurean.com/sharc for more
 * information.
 *
 * 03/07/13 14:55
 */

#include "client.h"

SHARC_FORCE_INLINE void sharc_client_exit_error(const char *message) {
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
    fprintf(stderr, "%c[1;31m", SHARC_ESCAPE_CHARACTER);
#endif
    fprintf(stderr, "Sharc error");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
    fprintf(stderr, "%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
    fprintf(stderr, " : %s\n", message);
    exit(0);
}

SHARC_FORCE_INLINE FILE *sharc_client_checkOpenFile(const char *fileName, const char *options, const sharc_bool checkOverwrite) {
    if (checkOverwrite && access(fileName, F_OK) != -1) {
        printf("File %s already exists. Do you want to overwrite it (y/N) ? ", fileName);
        switch (getchar()) {
            case 'y':
                break;
            default:
                exit(0);
        }
    }
    FILE *file = fopen(fileName, options);
    if (file == NULL) {
        char message[512];
        sprintf(message, "Unable to open file %s", fileName);
        sharc_client_exit_error(message);
    }
    return file;
}

SHARC_FORCE_INLINE void sharc_client_version() {
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
    printf("%c[1m", SHARC_ESCAPE_CHARACTER);
#endif
    printf("Centaurean Sharc %i.%i.%i", SHARC_MAJOR_VERSION, SHARC_MINOR_VERSION, SHARC_REVISION);
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
    printf("%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
    printf(" powered by Centaurean Density %i.%i.%i\n", density_version_major(), density_version_minor(), density_version_revision());
    printf("Copyright (C) 2013 Guillaume Voirin\n");
    printf("Built for %s (%s endian system, %u bits) using GCC %d.%d.%d, %s %s\n", SHARC_PLATFORM_STRING, SHARC_ENDIAN_STRING, (unsigned int) (8 * sizeof(void *)), __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, __DATE__, __TIME__);
}

SHARC_FORCE_INLINE void sharc_client_usage() {
    sharc_client_version();
    printf("\nSuperfast compression\n\n");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
    printf("%c[1m", SHARC_ESCAPE_CHARACTER);
#endif
    printf("Usage :\n");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
    printf("%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
    printf("  sharc [OPTIONS]... [FILES]...\n\n");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
    printf("%c[1m", SHARC_ESCAPE_CHARACTER);
#endif
    printf("Available options :\n");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
    printf("%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
    printf("  -c[LEVEL], --compress[=LEVEL]     Compress files using LEVEL if specified (default)\n");
    printf("                                    LEVEL can have the following values :\n");
    printf("                                    0 = No compression\n");
    printf("                                    1 = Chameleon algorithm, fastest compression available (default)\n");
    printf("                                    2 = Chameleon algorithm dual pass\n");
    printf("                                    3 = Argonaut algorithm\n");
    printf("  -d, --decompress                  Decompress files\n");
    printf("  -p[PATH], --output-path[=PATH]    Set output path\n");
    printf("  -n, --no-prompt                   Overwrite without prompting\n");
    printf("  -i, --stdin                       Read from stdin\n");
    printf("  -o, --stdout                      Write to stdout\n");
    printf("  -v, --version                     Display version information\n");
    printf("  -h, --help                        Display this help\n");
    exit(0);
}

void sharc_client_format_decimal(uint64_t number) {
    if (number < 1000) {
        printf("%"PRIu64, number);
        return;
    }
    sharc_client_format_decimal(number / 1000);
    printf(",%03"PRIu64, number % 1000);
}

SHARC_FORCE_INLINE uint_fast64_t sharc_client_reloadInputBuffer(density_stream *restrict stream, const sharc_client_io *restrict io_in) {
    stream->in.size = (uint_fast64_t) fread(stream->in.pointer, sizeof(sharc_byte), SHARC_PREFERRED_BUFFER_SIZE, io_in->stream);
    if (stream->in.size < SHARC_PREFERRED_BUFFER_SIZE) {
        if (ferror(io_in->stream))
            sharc_client_exit_error("Error reading file");
    }
    density_byte_buffer_rewind(&stream->in);
    return stream->in.size;
}

SHARC_FORCE_INLINE uint_fast64_t sharc_client_emptyOutputBuffer(density_stream *restrict stream, const sharc_client_io *restrict io_out) {
    uint_fast64_t written = (uint_fast64_t) fwrite(stream->out.pointer, sizeof(sharc_byte), (size_t) stream->out.position, io_out->stream);
    if (written < stream->out.position) {
        if (ferror(io_out->stream))
            sharc_client_exit_error("Error writing file");
    }
    density_byte_buffer_rewind(&stream->out);
    return written;
}

SHARC_FORCE_INLINE void sharc_client_actionRequired(uint_fast64_t *read, uint_fast64_t *written, const sharc_client_io *restrict io_in, const sharc_client_io *restrict io_out, density_stream *restrict stream, const DENSITY_STREAM_STATE streamState, const char *errorMessage) {
    switch (streamState) {
        case DENSITY_STREAM_STATE_STALL_ON_OUTPUT_BUFFER:
            *written = sharc_client_emptyOutputBuffer(stream, io_out);
            break;
        case DENSITY_STREAM_STATE_STALL_ON_INPUT_BUFFER:
            *read = sharc_client_reloadInputBuffer(stream, io_in);
            break;
        default:
            sharc_client_exit_error(errorMessage);
    }
}

SHARC_FORCE_INLINE void sharc_client_compress(sharc_client_io *io_in, sharc_client_io *io_out, const DENSITY_COMPRESSION_MODE attemptMode, const sharc_bool prompting, const char *inPath, const char *outPath) {
    struct stat attributes;

    const size_t inFileNameLength = strlen(io_in->name);
    char inFilePath[strlen(inPath) + inFileNameLength + 1];
    char outFilePath[strlen(outPath) + inFileNameLength + 6 + 1];
    sprintf(inFilePath, "%s%s", inPath, io_in->name);

    if (io_in->origin_type == SHARC_HEADER_ORIGIN_TYPE_FILE) {
        if (io_out->origin_type == SHARC_HEADER_ORIGIN_TYPE_FILE) {
            sprintf(outFilePath, "%s%s.sharc", outPath, io_in->name);

            io_out->name = outFilePath;
        }

        io_in->stream = sharc_client_checkOpenFile(inFilePath, "rb", false);

        stat(inFilePath, &attributes);
    } else {
        if (io_out->origin_type == SHARC_HEADER_ORIGIN_TYPE_FILE)
            io_out->name = SHARC_STDIN_COMPRESSED;

        io_in->stream = stdin;
        io_in->name = SHARC_STDIN;
    }

    if (io_out->origin_type == SHARC_HEADER_ORIGIN_TYPE_FILE)
        io_out->stream = sharc_client_checkOpenFile(io_out->name, "wb", prompting);
    else {
        io_out->stream = stdout;
        io_out->name = SHARC_STDOUT;
    }

    sharc_chrono chrono;
    sharc_chrono_start(&chrono);

    /*
     * The following code is an example of how to use the stream API to compress a file
     */
    uint64_t totalWritten = sharc_header_write(io_out->stream, io_out->origin_type, &attributes);
    density_stream *stream = (density_stream *) malloc(sizeof(density_stream));
    DENSITY_STREAM_STATE streamState;
    uint_fast64_t read = 0, written = 0;
    if (density_stream_prepare(stream, input_buffer, SHARC_PREFERRED_BUFFER_SIZE, output_buffer, SHARC_PREFERRED_BUFFER_SIZE, NULL, NULL))
        sharc_client_exit_error("Unable to prepare compression");
    read = sharc_client_reloadInputBuffer(stream, io_in);
    while ((streamState = density_stream_compress_init(stream, attemptMode, DENSITY_ENCODE_OUTPUT_TYPE_DEFAULT, DENSITY_BLOCK_TYPE_DEFAULT)))
        sharc_client_actionRequired(&read, &written, io_in, io_out, stream, streamState, "Unable to initialize compression");
    while ((streamState = density_stream_compress(stream, read < SHARC_PREFERRED_BUFFER_SIZE)))
        sharc_client_actionRequired(&read, &written, io_in, io_out, stream, streamState, "An error occured during compression");
    while ((streamState = density_stream_compress_finish(stream)))
        sharc_client_actionRequired(&read, &written, io_in, io_out, stream, streamState, "An error occured while finishing compression");
    sharc_client_emptyOutputBuffer(stream, io_out);
    /*
     * That's it !
     */

    sharc_chrono_stop(&chrono);

    if (io_out->origin_type == SHARC_HEADER_ORIGIN_TYPE_FILE) {
        const double elapsed = sharc_chrono_elapsed(&chrono);

        totalWritten += *stream->out_total_written;
        fclose(io_out->stream);

        if (io_in->origin_type == SHARC_HEADER_ORIGIN_TYPE_FILE) {
            uint64_t totalRead = *stream->in_total_read;
            fclose(io_in->stream);

            double ratio = (100.0 * totalWritten) / totalRead;
            double speed = (1.0 * totalRead) / (elapsed * 1024.0 * 1024.0);
            printf("Compressed ");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
            printf("%c[1m", SHARC_ESCAPE_CHARACTER);
#endif
            printf("%s", inFilePath);
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
            printf("%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
            printf(" (");
            sharc_client_format_decimal(totalRead);
            printf(" bytes) to ");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
            printf("%c[1m", SHARC_ESCAPE_CHARACTER);
#endif
            printf("%s", io_out->name);
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
            printf("%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
            printf(" (");
            sharc_client_format_decimal(totalWritten);
            printf(" bytes), ");
            printf("Ratio out / in = %.1lf%%, Time = %.3lf s, Speed = %.0lf MB/s\n", ratio, elapsed, speed);
        } else {
            printf("Compressed ");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
            printf("%c[1m", SHARC_ESCAPE_CHARACTER);
#endif
            printf("%s", io_in->name);
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
            printf("%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
            printf(" to ");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
            printf("%c[1m", SHARC_ESCAPE_CHARACTER);
#endif
            printf("%s", io_out->name);
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
            printf("%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
            printf(", %"PRIu64" bytes written.\n", totalWritten);
        }
    }
    free(stream);
}

SHARC_FORCE_INLINE void sharc_client_decompress(sharc_client_io *io_in, sharc_client_io *io_out, const sharc_bool prompting, const char *inPath, const char *outPath) {
    const size_t inFileNameLength = strlen(io_in->name);
    char inFilePath[strlen(inPath) + inFileNameLength];
    char outFilePath[strlen(outPath) + inFileNameLength - 6];
    sprintf(inFilePath, "%s%s", inPath, io_in->name);

    if (io_in->origin_type == SHARC_HEADER_ORIGIN_TYPE_FILE) {
        if (io_out->origin_type == SHARC_HEADER_ORIGIN_TYPE_FILE) {
            outFilePath[0] = '\0';
            strcat(outFilePath, outPath);
            strncat(outFilePath, io_in->name, inFileNameLength - 6);

            io_out->name = outFilePath;
        }

        io_in->stream = sharc_client_checkOpenFile(inFilePath, "rb", false);
    } else {
        if (io_out->origin_type == SHARC_HEADER_ORIGIN_TYPE_FILE)
            io_out->name = SHARC_STDIN;

        io_in->stream = stdin;
        io_in->name = SHARC_STDIN;
    }

    if (io_out->origin_type == SHARC_HEADER_ORIGIN_TYPE_FILE)
        io_out->stream = sharc_client_checkOpenFile(io_out->name, "wb", prompting);
    else {
        io_out->stream = stdout;
        io_out->name = SHARC_STDOUT;
    }

    sharc_chrono chrono;
    sharc_chrono_start(&chrono);

    /*
     * The following code is an example of how to use the stream API to decompress a file
     */
    sharc_header header;
    uint64_t totalRead = sharc_header_read(io_in->stream, &header);
    if (!sharc_header_check_validity(&header))
        sharc_client_exit_error("Invalid file");
    density_stream *stream = (density_stream *) malloc(sizeof(density_stream));
    DENSITY_STREAM_STATE streamState;
    uint_fast64_t read = 0, written = 0;
    if (density_stream_prepare(stream, input_buffer, SHARC_PREFERRED_BUFFER_SIZE, output_buffer, SHARC_PREFERRED_BUFFER_SIZE, NULL, NULL))
        sharc_client_exit_error("Unable to prepare decompression");
    read = sharc_client_reloadInputBuffer(stream, io_in);
    while ((streamState = density_stream_decompress_init(stream)))
        sharc_client_actionRequired(&read, &written, io_in, io_out, stream, streamState, "Unable to initialize decompression");
    while ((streamState = density_stream_decompress(stream, read < SHARC_PREFERRED_BUFFER_SIZE)))
        sharc_client_actionRequired(&read, &written, io_in, io_out, stream, streamState, "An error occured during decompression");
    while ((streamState = density_stream_decompress_finish(stream)))
        sharc_client_actionRequired(&read, &written, io_in, io_out, stream, streamState, "An error occured while finishing decompression");
    sharc_client_emptyOutputBuffer(stream, io_out);
    /*
     * That's it !
     */

    sharc_chrono_stop(&chrono);

    if (io_out->origin_type == SHARC_HEADER_ORIGIN_TYPE_FILE) {
        const double elapsed = sharc_chrono_elapsed(&chrono);

        uint64_t totalWritten = *stream->out_total_written;
        fclose(io_out->stream);

        if (header.genericHeader.originType == SHARC_HEADER_ORIGIN_TYPE_FILE)
            sharc_header_restore_file_attributes(&header, io_out->name);

        if (io_in->origin_type == SHARC_HEADER_ORIGIN_TYPE_FILE) {
            totalRead += *stream->in_total_read;
            fclose(io_in->stream);

            if (header.genericHeader.originType == SHARC_HEADER_ORIGIN_TYPE_FILE) {
                if (totalWritten != header.fileInformationHeader.originalFileSize)
                    sharc_client_exit_error("Input file is corrupt !");
            }

            double speed = (1.0 * totalWritten) / (elapsed * 1024.0 * 1024.0);
            printf("Decompressed ");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
            printf("%c[1m", SHARC_ESCAPE_CHARACTER);
#endif
            printf("%s", inFilePath);
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
            printf("%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
            printf(" (");
            sharc_client_format_decimal(totalRead);
            printf(" bytes) to ");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
            printf("%c[1m", SHARC_ESCAPE_CHARACTER);
#endif
            printf("%s", io_out->name);
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
            printf("%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
            printf(" (");
            sharc_client_format_decimal(totalWritten);
            printf(" bytes), ");
            printf("Time = %.3lf s, Speed = %.0lf MB/s\n", elapsed, speed);
        } else {
            printf("Decompressed ");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
            printf("%c[1m", SHARC_ESCAPE_CHARACTER);
#endif
            printf("%s", io_in->name);
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
            printf("%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
            printf(" to ");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
            printf("%c[1m", SHARC_ESCAPE_CHARACTER);
#endif
            printf("%s", io_out->name);
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
            printf("%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
            printf(", %"PRIu64" bytes written.\n", totalWritten);
        }
    }
    free(stream);
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif

    if (argc <= 1)
        sharc_client_usage();

    sharc_byte action = SHARC_ACTION_COMPRESS;
    DENSITY_COMPRESSION_MODE mode = DENSITY_COMPRESSION_MODE_CHAMELEON_ALGORITHM;
    sharc_byte prompting = SHARC_PROMPTING;
    sharc_client_io in;
    in.origin_type = SHARC_HEADER_ORIGIN_TYPE_FILE;
    in.name = "";
    sharc_client_io out;
    out.origin_type = SHARC_HEADER_ORIGIN_TYPE_FILE;
    out.name = "";
    sharc_byte pathMode = SHARC_FILE_OUTPUT_PATH;
    char inPath[SHARC_OUTPUT_PATH_MAX_SIZE] = "";
    char outPath[SHARC_OUTPUT_PATH_MAX_SIZE] = "";

    size_t argLength;
    for (int i = 1; i < argc; i++) {
        switch (argv[i][0]) {
            case '-':
                argLength = strlen(argv[i]);
                if (argLength < 2)
                    sharc_client_usage();
                switch (argv[i][1]) {
                    case 'c':
                        if (argLength == 2)
                            break;
                        if (argLength != 3)
                            sharc_client_usage();
                        switch (argv[i][2] - '0') {
                            case 0:
                                mode = DENSITY_COMPRESSION_MODE_COPY;
                                break;
                            case 1:
                                mode = DENSITY_COMPRESSION_MODE_CHAMELEON_ALGORITHM;
                                break;
                            case 2:
                                mode = DENSITY_COMPRESSION_MODE_MANDALA_ALGORITHM;
                                break;
                            /*case 3:
                                mode = DENSITY_COMPRESSION_MODE_ARGONAUT;
                                break;*/
                            default:
                                sharc_client_usage();
                        }
                        break;
                    case 'd':
                        action = SHARC_ACTION_DECOMPRESS;
                        break;
                    case 'p':
                        if (argLength == 2)
                            sharc_client_usage();
                        char *lastSeparator = strrchr(argv[i], SHARC_PATH_SEPARATOR);
                        if (argv[i][2] != '.') {
                            if (lastSeparator == NULL)
                                sharc_client_usage();
                            if (lastSeparator - argv[i] + 1 != argLength)
                                sharc_client_usage();
                            strncpy(outPath, argv[i] + 2, SHARC_OUTPUT_PATH_MAX_SIZE);
                        }
                        pathMode = SHARC_FIXED_OUTPUT_PATH;
                        break;
                    case 'n':
                        prompting = SHARC_NO_PROMPTING;
                        break;
                    case 'i':
                        in.origin_type = SHARC_HEADER_ORIGIN_TYPE_STREAM;
                        break;
                    case 'o':
                        out.origin_type = SHARC_HEADER_ORIGIN_TYPE_STREAM;
                        break;
                    case 'v':
                        sharc_client_version();
                        exit(0);
                        break;
                    case 'h':
                        sharc_client_usage();
                        break;
                    case '-':
                        if (argLength < 3)
                            sharc_client_usage();
                        switch (argv[i][2]) {
                            case 'c':
                                if (argLength == 10)
                                    break;
                                if (argLength != 12)
                                    sharc_client_usage();
                                switch (argv[i][11] - '0') {
                                    case 0:
                                        mode = DENSITY_COMPRESSION_MODE_COPY;
                                        break;
                                    case 1:
                                        mode = DENSITY_COMPRESSION_MODE_CHAMELEON_ALGORITHM;
                                        break;
                                    case 2:
                                        mode = DENSITY_COMPRESSION_MODE_MANDALA_ALGORITHM;
                                        break;
                                    /*case 3:
                                        mode = DENSITY_COMPRESSION_MODE_ARGONAUT;
                                        break;*/
                                    default:
                                        sharc_client_usage();
                                }
                                break;
                            case 'd':
                                if (argLength != 12)
                                    sharc_client_usage();
                                action = SHARC_ACTION_DECOMPRESS;
                                break;
                            case 'o':
                                if (argLength <= 14)
                                    sharc_client_usage();
                                lastSeparator = strrchr(argv[i], SHARC_PATH_SEPARATOR);
                                if (argv[i][14] != '.') {
                                    if (lastSeparator == NULL)
                                        sharc_client_usage();
                                    if (lastSeparator - argv[i] + 1 != argLength)
                                        sharc_client_usage();
                                    strncpy(outPath, argv[i] + 14, SHARC_OUTPUT_PATH_MAX_SIZE);
                                }
                                pathMode = SHARC_FIXED_OUTPUT_PATH;
                                break;
                            case 'n':
                                if (argLength != 11)
                                    sharc_client_usage();
                                prompting = SHARC_NO_PROMPTING;
                                break;
                            case 's':
                                if (argLength == 7)
                                    in.origin_type = SHARC_HEADER_ORIGIN_TYPE_STREAM;
                                else if (argLength == 8)
                                    out.origin_type = SHARC_HEADER_ORIGIN_TYPE_STREAM;
                                else
                                    sharc_client_usage();
                                break;
                            case 'v':
                                if (argLength != 9)
                                    sharc_client_usage();
                                sharc_client_version();
                                exit(0);
                            case 'h':
                                sharc_client_usage();
                                break;
                            default:
                                sharc_client_usage();
                        }
                        break;

                    default:
                        break;
                }
                break;
            default:
                if (in.origin_type == SHARC_HEADER_ORIGIN_TYPE_FILE) {
                    char *lastSeparator = strrchr(argv[i], SHARC_PATH_SEPARATOR);
                    if (lastSeparator != NULL) {
                        in.name = lastSeparator + 1;
                        size_t charsToCopy = in.name - argv[i];
                        if (charsToCopy < SHARC_OUTPUT_PATH_MAX_SIZE) {
                            strncpy(inPath, argv[i], charsToCopy);
                            inPath[charsToCopy] = '\0';
                        } else
                            sharc_client_usage();
                        if (pathMode == SHARC_FILE_OUTPUT_PATH)
                        strcpy(outPath, inPath);
                    } else
                        in.name = argv[i];
                }
                switch (action) {
                    case SHARC_ACTION_DECOMPRESS:
                        sharc_client_decompress(&in, &out, prompting, inPath, outPath);
                        break;
                    default:
                        sharc_client_compress(&in, &out, mode, prompting, inPath, outPath);
                        break;
                }
                break;
        }
    }

    if (in.origin_type == SHARC_HEADER_ORIGIN_TYPE_STREAM) {
        switch (action) {
            case SHARC_ACTION_DECOMPRESS:
                sharc_client_decompress(&in, &out, prompting, inPath, outPath);
                break;
            default:
                sharc_client_compress(&in, &out, mode, prompting, inPath, outPath);
                break;
        }
    }

    return true;
}


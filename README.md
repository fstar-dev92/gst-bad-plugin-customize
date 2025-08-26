# MPEGTSMUX Custom PID and Service ID Support

This implementation adds the ability to set custom PIDs for audio/video streams, PMT PID, and service ID (transport stream ID) in the GStreamer mpegtsmux element.

## New Properties

### audio-pid
- **Type**: uint
- **Range**: 0 or 0x0100-0x1FFE
- **Default**: 0x0100
- **Description**: PID to use for audio streams. Set to 0 for auto-assignment.

### video-pid
- **Type**: uint
- **Range**: 0 or 0x0100-0x1FFE
- **Default**: 0x0101
- **Description**: PID to use for video streams. Set to 0 for auto-assignment.

### pmt-pid
- **Type**: uint
- **Range**: 0 or 0x0020-0x1FFE
- **Default**: 0x1000
- **Description**: PID to use for PMT (Program Map Table). Set to 0 for auto-assignment.

### service-id
- **Type**: uint
- **Range**: 0x0001-0xFFFF
- **Default**: 0x0001
- **Description**: Service ID (Transport Stream ID) for the PAT (Program Association Table).

## Usage Examples

### GStreamer Pipeline
```bash
# Set custom PIDs and service ID
gst-launch-1.0 \
  videotestsrc ! video/x-raw,width=640,height=480 ! x264enc ! mpegtsmux name=mux audio-pid=0x0100 video-pid=0x0101 pmt-pid=0x1000 service-id=0x1234 ! filesink location=output.ts

# Audio and video with custom PIDs
gst-launch-1.0 \
  audiotestsrc ! audioconvert ! avenc_aac ! mux. \
  videotestsrc ! video/x-raw,width=640,height=480 ! x264enc ! mux. \
  mpegtsmux name=mux audio-pid=0x0200 video-pid=0x0201 pmt-pid=0x2000 service-id=0x5678 ! filesink location=output.ts
```

### C Code
```c
GstElement *muxer = gst_element_factory_make("mpegtsmux", "muxer");

// Set custom PIDs and service ID
g_object_set(muxer, "audio-pid", 0x0100, NULL);
g_object_set(muxer, "video-pid", 0x0101, NULL);
g_object_set(muxer, "pmt-pid", 0x1000, NULL);
g_object_set(muxer, "service-id", 0x1234, NULL);

// Get current values
guint audio_pid, video_pid, pmt_pid, service_id;
g_object_get(muxer, "audio-pid", &audio_pid, NULL);
g_object_get(muxer, "video-pid", &video_pid, NULL);
g_object_get(muxer, "pmt-pid", &pmt_pid, NULL);
g_object_get(muxer, "service-id", &service_id, NULL);
```

## Implementation Details

- **PID Assignment**: Audio and video PIDs are assigned when new sink pads are created, based on the media type detected from the caps.
- **PMT PID**: Set when the tsmux is created or when the property is changed.
- **Service ID**: Set as the transport stream ID in the PAT table.
- **Validation**: All PIDs and service IDs are validated against MPEG-TS specifications.

## Limitations

- Audio and video PID changes only affect new streams; existing streams retain their assigned PIDs.
- PMT PID and service ID changes take effect immediately if the muxer is already running.
- PID conflicts are detected and reported as errors.

## Building and Testing

```bash
# Build the test program
make

# Run the test
./test_mpegtsmux

# Clean up
make clean
```

## Notes

- Setting a PID to 0 enables automatic PID assignment (default behavior).
- Valid PID ranges follow MPEG-TS specifications:
  - Elementary stream PIDs: 0x0100-0x1FFE
  - PMT PIDs: 0x0020-0x1FFE
  - Reserved PIDs (0x0000-0x001F, 0x1FFF) are not allowed
- The service ID must be unique within a transport stream.

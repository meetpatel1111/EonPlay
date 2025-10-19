#ifndef VLC_VLC_H
#define VLC_VLC_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libvlc_instance_t libvlc_instance_t;
typedef struct libvlc_media_t libvlc_media_t;
typedef struct libvlc_media_player_t libvlc_media_player_t;
typedef struct libvlc_event_manager_t libvlc_event_manager_t;
typedef struct libvlc_event_t libvlc_event_t;
typedef struct libvlc_media_track_t libvlc_media_track_t;
typedef long long libvlc_time_t;
typedef int libvlc_media_parsed_status_t;

enum libvlc_track_type_t {
    libvlc_track_unknown = -1,
    libvlc_track_audio = 0,
    libvlc_track_video = 1,
    libvlc_track_text = 2
};

enum {
    libvlc_media_parsed_status_skipped = 1,
    libvlc_media_parsed_status_failed = 2,
    libvlc_media_parsed_status_timeout = 3,
    libvlc_media_parsed_status_done = 4
};

struct libvlc_media_track_t {
    unsigned int i_codec;
    unsigned int i_original_fourcc;
    int i_id;
    enum libvlc_track_type_t i_type;
    int i_profile;
    int i_level;
    union {
        struct {
            unsigned int i_bitrate;
            unsigned int i_channels;
            unsigned int i_rate;
        } audio;
        struct {
            unsigned int i_height;
            unsigned int i_width;
            unsigned int i_sar_num;
            unsigned int i_sar_den;
            unsigned int i_frame_rate_num;
            unsigned int i_frame_rate_den;
        } video;
    };
    unsigned int i_bitrate;
    char* psz_language;
    char* psz_description;
    char* psz_codec;
};

// Event types
enum libvlc_event_e {
    libvlc_MediaPlayerPlaying = 0x100,
    libvlc_MediaPlayerPaused,
    libvlc_MediaPlayerStopped,
    libvlc_MediaPlayerEndReached,
    libvlc_MediaPlayerEncounteredError,
    libvlc_MediaPlayerBuffering,
    libvlc_MediaPlayerLengthChanged
};

// State types
enum libvlc_state_t {
    libvlc_NothingSpecial = 0,
    libvlc_Opening,
    libvlc_Buffering,
    libvlc_Playing,
    libvlc_Paused,
    libvlc_Stopped,
    libvlc_Ended,
    libvlc_Error
};

// Event structure
struct libvlc_event_t {
    int type;
    void* p_obj;
    union {
        struct {
            float new_cache;
        } media_player_buffering;
    } u;
};

// Event callback
typedef void (*libvlc_callback_t)(const libvlc_event_t* p_event, void* p_data);

// Basic VLC functions for compilation
libvlc_instance_t* libvlc_new(int argc, const char* const* argv);
void libvlc_release(libvlc_instance_t* p_instance);
const char* libvlc_get_version(void);

// Media functions
libvlc_media_t* libvlc_media_new_path(libvlc_instance_t* p_instance, const char* path);
libvlc_media_t* libvlc_media_new_location(libvlc_instance_t* p_instance, const char* psz_mrl);
void libvlc_media_release(libvlc_media_t* p_media);
void libvlc_media_parse(libvlc_media_t* p_media);
libvlc_media_parsed_status_t libvlc_media_get_parsed_status(libvlc_media_t* p_media);
libvlc_time_t libvlc_media_get_duration(libvlc_media_t* p_media);
char* libvlc_media_get_mrl(libvlc_media_t* p_media);
unsigned int libvlc_media_tracks_get(libvlc_media_t* p_media, libvlc_media_track_t*** pp_tracks);
void libvlc_media_tracks_release(libvlc_media_track_t** p_tracks, unsigned int i_count);

// Media player functions
libvlc_media_player_t* libvlc_media_player_new(libvlc_instance_t* p_libvlc_instance);
libvlc_media_player_t* libvlc_media_player_new_from_media(libvlc_media_t* p_media);
void libvlc_media_player_release(libvlc_media_player_t* p_mi);
void libvlc_media_player_set_media(libvlc_media_player_t* p_mi, libvlc_media_t* p_media);
int libvlc_media_player_play(libvlc_media_player_t* p_mi);
void libvlc_media_player_pause(libvlc_media_player_t* p_mi);
void libvlc_media_player_stop(libvlc_media_player_t* p_mi);
libvlc_time_t libvlc_media_player_get_time(libvlc_media_player_t* p_mi);
void libvlc_media_player_set_time(libvlc_media_player_t* p_mi, libvlc_time_t i_time);
libvlc_time_t libvlc_media_player_get_length(libvlc_media_player_t* p_mi);
libvlc_state_t libvlc_media_player_get_state(libvlc_media_player_t* p_mi);

// Audio functions
int libvlc_audio_set_volume(libvlc_media_player_t* p_mi, int i_volume);

// Event manager functions
libvlc_event_manager_t* libvlc_media_player_event_manager(libvlc_media_player_t* p_mi);
int libvlc_event_attach(libvlc_event_manager_t* p_event_manager, libvlc_event_e i_event_type, libvlc_callback_t f_callback, void* user_data);

// Memory functions
void libvlc_free(void* ptr);

#ifdef __cplusplus
}
#endif

#endif // VLC_VLC_H
/*
 *  Tvheadend
 *  Copyright (C) 2010 Andreas Öman
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SERVICE_H__
#define SERVICE_H__

#include "htsmsg.h"
#include "idnode.h"
#include "descrambler.h"

extern const idclass_t service_class;

extern struct service_queue service_all;

struct channel;

/**
 * Stream, one media component for a service.
 */
typedef struct elementary_stream {

  TAILQ_ENTRY(elementary_stream) es_link;
  TAILQ_ENTRY(elementary_stream) es_filt_link;
  int es_position;
  struct service *es_service;

  streaming_component_type_t es_type;
  int es_index;

  uint16_t es_aspect_num;
  uint16_t es_aspect_den;

  char es_lang[4];           /* ISO 639 2B 3-letter language code */
  uint8_t es_audio_type;     /* Audio type */

  uint16_t es_composition_id;
  uint16_t es_ancillary_id;

  int16_t es_pid;
  uint16_t es_parent_pid;    /* For subtitle streams originating from 
				a teletext stream. this is the pid
				of the teletext stream */
  int8_t es_pid_opened;      /* PID is opened */

  int8_t es_cc;             /* Last CC */

  avgstat_t es_cc_errors;
  avgstat_t es_rate;

  int es_peak_presentation_delay; /* Max seen diff. of DTS and PTS */

  /* PCR recovery */

  int es_pcr_recovery_fails;
  int64_t es_pcr_real_last;     /* realtime clock when we saw last PCR */
  int64_t es_pcr_last;          /* PCR clock when we saw last PCR */
  int64_t es_pcr_drift;

  /* For service stream packet reassembly */

  sbuf_t es_buf;

  uint32_t es_startcond;
  uint32_t es_startcode;
  uint32_t es_startcode_offset;
  int es_parser_state;
  int es_parser_ptr;
  void *es_priv;          /* Parser private data */

  sbuf_t es_buf_ps;       // program stream reassembly (analogue adapters)
  sbuf_t es_buf_a;        // Audio packet reassembly

  uint8_t *es_global_data;
  int es_global_data_len;
  int es_incomplete;
  int es_ssc_intercept;
  int es_ssc_ptr;
  uint8_t es_ssc_buf[32];

  struct th_pkt *es_curpkt;
  int64_t es_curpts;
  int64_t es_curdts;
  int64_t es_prevdts;
  int64_t es_nextdts;
  int es_frame_duration;
  int es_width;
  int es_height;

  int es_meta_change;

  /* CA ID's on this stream */
  struct caid_list es_caids;

  int es_vbv_size;        /* Video buffer size (in bytes) */
  int es_vbv_delay;       /* -1 if CBR */

  /* */

  int es_delete_me;      /* Temporary flag for deleting streams */

  /* Error log limiters */

  loglimiter_t es_loglimit_cc;
  loglimiter_t es_loglimit_pes;
  
  char *es_nicename;

  /* Teletext subtitle */ 
  char es_blank; // Last subtitle was blank

  /* SI section processing (horrible hack) */
  void *es_section;

  /* Filter temporary variable */
  uint32_t es_filter;

} elementary_stream_t;


typedef TAILQ_HEAD(service_instance_list, service_instance) service_instance_list_t;

/**
 *
 */
typedef struct service_instance {

  TAILQ_ENTRY(service_instance) si_link;

  int si_prio;

  struct service *si_s; // A reference is held
  int si_instance;       // Discriminator when having multiple adapters, etc

  int si_error;        /* Set if subscription layer deem this cand
                          to be broken. We typically set this if we
                          have not seen any demuxed packets after
                          the grace period has expired.
                          The actual value is current time
                       */

  time_t si_error_time;


  int si_weight;         // Highest weight that holds this cand

  int si_mark;           // For mark & sweep

} service_instance_t;


/**
 *
 */
service_instance_t *service_instance_add(service_instance_list_t *sil,
                                         struct service *s,
                                         int instance,
                                         int prio,
                                         int weight);

void service_instance_destroy
  (service_instance_list_t *sil, service_instance_t *si);

void service_instance_list_clear(service_instance_list_t *sil);

/**
 *
 */
typedef struct service {
  idnode_t s_id;

  TAILQ_ENTRY(service) s_all_link;

  enum {
    /**
     * Transport is idle.
     */
    SERVICE_IDLE,

    /**
     * Transport producing output
     */
    SERVICE_RUNNING,

    /**
     * Destroyed, but pointer is till valid. 
     * This would be the case if transport_destroy() did not actually free 
     * the transport because there are references held to it.
     *
     * Reference counts can be used so that code can hold a pointer to 
     * a transport without having the global lock.
     *
     * Note: No fields in the transport may be accessed without the
     * global lock held. Thus, the global_lock must be reaquired and
     * then s_status must be checked. If it is ZOMBIE the code must
     * just drop the refcount and pretend that the transport never
     * was there in the first place.
     */
    SERVICE_ZOMBIE, 
  } s_status;

  /**
   * Refcount, operated using atomic.h ops.
   */ 
  int s_refcount;

  /**
   *
   */
  int s_flags;

#define S_DEBUG 0x1

  /**
   * Source type is used to determine if an output requesting
   * MPEG-TS can shortcut all the parsing and remuxing.
   */ 
  enum {
    S_MPEG_TS,
    S_MPEG_PS,
    S_OTHER,
  } s_source_type;

  /**
   * Service type
   */
  enum {
    ST_NONE,
    ST_OTHER,
    ST_SDTV,
    ST_HDTV,
    ST_RADIO
  } s_servicetype;

// TODO: should this really be here?

  /**
   * PID carrying the programs PCR.
   * XXX: We don't support transports that does not carry
   * the PCR in one of the content streams.
   */
  uint16_t s_pcr_pid;

  /**
   * PID for the PMT of this MPEG-TS stream.
   */
  uint16_t s_pmt_pid;

  /**
   * Set if transport is enabled (the default).  If disabled it should
   * not be considered when chasing for available transports during
   * subscription scheduling.
   */
  int s_enabled;

  LIST_ENTRY(service) s_active_link;

  LIST_HEAD(, th_subscription) s_subscriptions;

  int (*s_is_enabled)(struct service *t);

  void (*s_enlist)(struct service *s, service_instance_list_t *sil);

  int (*s_start_feed)(struct service *s, int instance);

  void (*s_refresh_feed)(struct service *t);

  void (*s_stop_feed)(struct service *t);

  void (*s_config_save)(struct service *t);

  void (*s_setsourceinfo)(struct service *t, struct source_info *si);

  int (*s_grace_period)(struct service *t);

  void (*s_delete)(struct service *t, int delconf);

  /**
   * Channel info
   */
  int         (*s_channel_number) (struct service *);
  const char *(*s_channel_name)   (struct service *);
  const char *(*s_provider_name)  (struct service *);

  /**
   * Name usable for displaying to user
   */
  char *s_nicename;
  int   s_nicename_prefidx;

  /**
   * Teletext...
   */
  th_commercial_advice_t s_tt_commercial_advice;
  time_t s_tt_clock;   /* Network clock as determined by teletext decoder */
 
  /**
   * Channel mapping
   */
  LIST_HEAD(,channel_service_mapping) s_channels;

  /**
   * Service mapping, see service_mapper.c form details
   */
  int s_sm_onqueue;
  TAILQ_ENTRY(service) s_sm_link;

  /**
   * Pending save.
   *
   * transport_request_save() will enqueue the transport here.
   * We need to do this if we don't hold the global lock.
   * This happens when we update PMT from within the TS stream itself.
   * Then we hold the stream mutex, and thus, can not obtain the global lock
   * as it would cause lock inversion.
   */
  int s_ps_onqueue;
  TAILQ_ENTRY(service) s_ps_link;

  /**
   * Timer which is armed at transport start. Once it fires
   * it will check if any packets has been parsed. If not the status
   * will be set to TRANSPORT_STATUS_NO_INPUT
   */
  gtimer_t s_receive_timer;
  /**
   * Stream start time
   */
  time_t s_start_time;


  /*********************************************************
   *
   * Streaming part of transport
   *
   * All access to fields below this must be protected with
   * s_stream_mutex held.
   *
   * Note: Code holding s_stream_mutex should _never_ 
   * acquire global_lock while already holding s_stream_mutex.
   *
   */

  /**
   * Mutex to be held during streaming.
   * This mutex also protects all elementary_stream_t instances for this
   * transport.
   */
  pthread_mutex_t s_stream_mutex;


  /**
   * Condition variable to singal when streaming_status changes
   * interlocked with s_stream_mutex
   */
  pthread_cond_t s_tss_cond;
  /**
   *
   */			   
  int s_streaming_status;

  // Progress
#define TSS_INPUT_HARDWARE   0x1
#define TSS_INPUT_SERVICE    0x2
#define TSS_MUX_PACKETS      0x4
#define TSS_PACKETS          0x8

#define TSS_GRACEPERIOD      0x8000

  // Errors
#define TSS_NO_DESCRAMBLER   0x10000
#define TSS_NO_ACCESS        0x20000
#define TSS_TIMEOUT          0x40000

#define TSS_ERRORS           0xffff0000


  /**
   *
   */
  int s_streaming_live;

  // Live status
#define TSS_LIVE             0x01

  /**
   * For simple streaming sources (such as video4linux) keeping
   * track of the video and audio stream is convenient.
   */
#ifdef MOVE_TO_V4L
  elementary_stream_t *s_video;
  elementary_stream_t *s_audio;
#endif
 
  /**
   * Average bitrate
   */
  avgstat_t s_rate;

  /**
   * Descrambling support
   */

  struct th_descrambler_list s_descramblers;
  uint16_t s_scrambled_seen;
  th_descrambler_runtime_t *s_descramble;

  /**
   * List of all and filtered components.
   */
  struct elementary_stream_queue s_components;
  struct elementary_stream_queue s_filt_components;
  int s_last_pid;
  elementary_stream_t *s_last_es;


  /**
   * Delivery pad, this is were we finally deliver all streaming output
   */
  streaming_pad_t s_streaming_pad;


  loglimiter_t s_loglimit_tei;


  int64_t s_current_pts;

} service_t;





void service_init(void);
void service_done(void);

int service_start(service_t *t, int instance);
void service_stop(service_t *t);

void service_build_filter(service_t *t);

service_t *service_create0(service_t *t, const idclass_t *idc, const char *uuid, int source_type, htsmsg_t *conf);

#define service_create(t, c, u, s, m)\
  (struct t*)service_create0(calloc(1, sizeof(struct t), &t##_class, c, u, s, m)

void service_unref(service_t *t);

void service_ref(service_t *t);

service_t *service_find(const char *identifier);
#define service_find_by_identifier service_find

service_instance_t *service_find_instance(struct service *s,
                                          struct channel *ch,
                                          service_instance_list_t *sil,
                                          int *error,
                                          int weight);

elementary_stream_t *service_stream_find_(service_t *t, int pid);

static inline elementary_stream_t *
service_stream_find(service_t *t, int pid)
{
  if (t->s_last_pid != (pid))
    return service_stream_find_(t, pid);
  else
    return t->s_last_es;
}

elementary_stream_t *service_stream_create(service_t *t, int pid,
				     streaming_component_type_t type);

void service_settings_write(service_t *t);

const char *service_servicetype_txt(service_t *t);

int service_is_sdtv(service_t *t);
int service_is_hdtv(service_t *t);
int service_is_radio(service_t *t);
int service_is_other(service_t *t);
#define service_is_tv(s) (service_is_hdtv(s) || service_is_sdtv(s))

int service_is_encrypted ( service_t *t );

void service_destroy(service_t *t, int delconf);

void service_remove_subscriber(service_t *t, struct th_subscription *s,
			       int reason);

void service_set_streaming_status_flags_(service_t *t, int flag);

static inline void
service_set_streaming_status_flags(service_t *t, int flag)
{
  if ((t->s_streaming_status & flag) != flag)
    service_set_streaming_status_flags_(t, flag);
}

struct streaming_start;
struct streaming_start *service_build_stream_start(service_t *t);

void service_restart(service_t *t, int had_components);

void service_stream_destroy(service_t *t, elementary_stream_t *st);

void service_request_save(service_t *t, int restart);

void service_source_info_free(source_info_t *si);

void service_source_info_copy(source_info_t *dst, const source_info_t *src);

void service_make_nicename(service_t *t);

const char *service_nicename(service_t *t);

const char *service_component_nicename(elementary_stream_t *st);

const char *service_adapter_nicename(service_t *t);

const char *service_tss2text(int flags);

static inline int service_tss_is_error(int flags)
{
  return flags & TSS_ERRORS ? 1 : 0;
}

void service_refresh_channel(service_t *t);

int tss2errcode(int tss);

uint16_t service_get_encryption(service_t *t);

htsmsg_t *servicetype_list (void);

void service_load ( service_t *s, htsmsg_t *c );

void service_save ( service_t *s, htsmsg_t *c );

void sort_elementary_streams(service_t *t);

const char *service_get_channel_name (service_t *s);
const char *service_get_full_channel_name (service_t *s);
int         service_get_channel_number (service_t *s);

#endif // SERVICE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>

#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <stdint.h>
#include <byteswap.h>
#include <math.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <inttypes.h>
#include <endian.h>
#include <getopt.h>
#include <limits.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <infiniband/verbs_exp.h>



#define CPU_UTILITY "/proc/stat"

/* Connection types available. */
#define RC  (0)
#define UC  (1)
#define UD  (2)
#define DC  (5)

/* Genral control definitions */
#define OFF	     (0)
#define ON 	     (1)
#define SUCCESS	     (0)
#define FAILURE	     (1)
// #define VERSION_EXIT (10)
#define HELP_EXIT	 (11)
#define MTU_FIX	     (7)
// #define MAX_SIZE     (8388608)
#define LINK_FAILURE (4)
// #define MAX_OUT_READ_HERMON (16)
// #define MAX_OUT_READ        (4)
#define UD_ADDITION         (40)
// #define RAWETH_ADDITION    (18)
// #define HW_CRC_ADDITION    (4)

/* Outstanding reads for "read" verb only. */
#define MAX_SEND_SGE		(1)
#define MAX_RECV_SGE		(1)
// #define CTX_POLL_BATCH		(16)
// #define PL			(1)
#define ATOMIC_ADD_VALUE	(1)
#define ATOMIC_SWAP_VALUE	(7)

/* Space for GRH when we scatter the packet in UD. */
// #define PINGPONG_SEND_WRID	(60)
// #define PINGPONG_RDMA_WRID	(3)
// #define PINGPONG_READ_WRID	(1)
// #define PINGPONG_ATOMIC_WRID	(22)
#define DEFF_QKEY		(0x11111111)

/* Default Values of perftest parameters */
#define DEF_PORT      (18515)
#define DEF_IB_PORT   (1)
#define DEF_IB_PORT2  (2)
#define DEF_SIZE_BW   (65536)
#define DEF_SIZE_LAT  (2)
#define DEF_ITERS     (1000)
// #define DEF_ITERS_WB  (5000)
// #define DEF_TX_BW     (128)
#define DEF_TX_LAT    (1)
#define DEF_QP_TIME   (14)
#define DEF_SL	      (0)
#define DEF_GID_INDEX (-1)
#define DEF_NUM_QPS   (1)
#define DEF_RX_RDMA   (1)
#define DEF_RX_SEND   (512)
#define DEF_CQ_MOD    (100)
#define DEF_SIZE_ATOMIC (8)
#define DEF_QKEY      0x11111111
// #define DEF_DURATION  (5)
// #define	DEF_MARGIN    (2)
#define DEF_INIT_MARGIN (-1)
#define DEF_INLINE    (-1)
#define DEF_TOS       (-1)
#define DEF_RETRY_COUNT (7)
#define DEF_CACHE_LINE_SIZE (64)
#define DEF_PAGE_SIZE (4096)

/* Optimal Values for Inline */
#define DEF_INLINE_WRITE (220)
#define DEF_INLINE_SEND_RC_UC (236)
// #define DEF_INLINE_SEND_XRC (236)
#define DEF_INLINE_SEND_UD (188)
#define DEF_INLINE_DC (150)

/* Max and Min allowed values for perftest parameters. */
#define MIN_TOS		(0)
#define MAX_TOS		(256)
#define MIN_IB_PORT   (1)
#define MAX_IB_PORT   (3)
#define MIN_ITER      (5)
#define MAX_ITER      (100000000)
#define MIN_TX 	      (1)
#define MAX_TX	      (15000)
#define MIN_SL	      (0)
#define MAX_SL	      (15)
#define MIN_GID_IX    (0)
#define MAX_GID_IX    (64)
#define MIN_QP_NUM    (1)
#define MAX_QP_NUM    (16384)
#define MIN_QP_MCAST  (1)
#define MAX_QP_MCAST  (56)
#define MIN_RX	      (1)
#define MAX_RX	      (16384)
#define UC_MAX_RX     (16000)
#define MIN_CQ_MOD    (1)
#define MAX_CQ_MOD    (1024)
#define MAX_INLINE    (912)
#define MAX_INLINE_UD (884)

/* Raw etherent defines */
// #define RAWETH_MIN_MSG_SIZE	(64)
// #define MIN_MTU_RAW_ETERNET	(64)
// #define MAX_MTU_RAW_ETERNET	(9600)

#define MAC_LEN (17)
#define ETHERTYPE_LEN (6)
#define MAC_ARR_LEN (6)
#define HEX_BASE (16)

#define KEY_MSG_SIZE 	 (59)   /* Message size without gid. */
#define KEY_MSG_SIZE_GID (108)   /* Message size with gid (MGID as well). */

#define CHECK_VALUE(arg,type,minv,maxv,name) 						    					\
{ arg = (type)strtol(optarg, NULL, 0); if ((arg < minv) || (arg > maxv))                \
	{ fprintf(stderr," %s should be between %d and %d\n",name,minv,maxv); return 1; }}

/* Macro for allocating. */
#define ALLOCATE(var,type,size)                                     \
{ if((var = (type*)malloc(sizeof(type)*(size))) == NULL)        \
	{ fprintf(stderr," Cannot Allocate\n"); exit(1);}}

/* This is our string builder */
#define GET_STRING(orig,temp) 						            \
{ ALLOCATE(orig,char,(strlen(temp) + 1)); strcpy(orig,temp); }

/* Macro to determine packet size in case of UD. The UD addition is for the GRH . */
#define SIZE(type,size,valid) ((type == UD && valid) ? (size + UD_ADDITION) : (size))


/* Macro to define the buffer size (according to "Nahalem" chip set).
 * for small message size (under 4K) , we allocate 4K buffer , and the RDMA write
 * verb will write in cycle on the buffer. this improves the BW in "Nahalem" systems.
 */
#define BUFF_SIZE(size,cycle_buffer) ((size < cycle_buffer) ? (cycle_buffer) : (size))

#define MTU_SIZE(mtu_ind) (((uint64_t)1 << (MTU_FIX + mtu_ind)))


/* The Basic print format for all verbs. */
#define BASIC_ADDR_FMT " %s address: LID %#04x QPN %#06x PSN %#06x"

/* Addition format string for READ - the outstanding reads. */
#define READ_FMT       " OUT %#04x"

/* The print format of the pingpong_dest element for RDMA verbs. */
#define RDMA_FMT       " RKey %#08x VAddr %#016Lx"

/* The print format of a global address or a multicast address. */
#define PERF_GID_FMT " %s: %02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d:%02d\n"

/* The Format of the message we pass through sockets , without passing Gid. */
#define KEY_PRINT_FMT "%04x:%04x:%06x:%06x:%08x:%016Lx:%08x"

/* The Format of the message we pass through sockets (With Gid). */
#define KEY_PRINT_FMT_GID "%04x:%04x:%06x:%06x:%08x:%016Lx:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%08x:"

#define NOTIFY_COMP_ERROR_SEND(wc,scnt,ccnt)                     											\
	{ fprintf(stderr," Completion with error at client\n");      											\
	  fprintf(stderr," Failed status %d: wr_id %d syndrom 0x%x\n",wc.status,(int) wc.wr_id,wc.vendor_err);	\
	  fprintf(stderr, "scnt=%lu, ccnt=%lu\n",scnt, ccnt); }

#define NOTIFY_COMP_ERROR_RECV(wc,rcnt)                     											    \
	{ fprintf(stderr," Completion with error at server\n");      											\
	  fprintf(stderr," Failed status %d: wr_id %d syndrom 0x%x\n",wc.status,(int) wc.wr_id,wc.vendor_err);	\
	  fprintf(stderr," rcnt=%lu\n",rcnt); }
	  
	  
	  
#define RESULT_LINE "---------------------------------------------------------------------------------------\n"

/* The Verb of the benchmark. */
typedef enum { SEND , WRITE, READ, ATOMIC } VerbType;

/* The type of the test */
typedef enum { LAT , BW } TestType;

/* The type of the machine ( server or client actually). */
typedef enum { SERVER , CLIENT , UNCHOSEN} MachineType;

/* The type of the machine ( server or client actually). */
typedef enum { LOCAL , REMOTE } PrintDataSide;

/* The atomic test type */
typedef enum {CMP_AND_SWAP, FETCH_AND_ADD} AtomicType;

/* Type of test method. */
typedef enum { ITERATIONS , DURATION } TestMethod;

/* for duration calculation */
// typedef enum { START_STATE, SAMPLE_STATE, STOP_SAMPLE_STATE, END_STATE} DurationStates;

/* Units for rate limiter */
enum rate_limiter_units {MEGA_BYTE_PS, GIGA_BIT_PS, PACKET_PS};

/* Verbosity Levels for test report */
enum verbosity_level {FULL_VERBOSITY=-1, OUTPUT_BW=0, OUTPUT_MR, OUTPUT_LAT };

/* Report format (Gbit/s VS MB/s) */
enum ctx_report_fmt { GBS, MBS };

/* Test method */
enum ctx_test_method {RUN_REGULAR, RUN_ALL, RUN_INFINITELY};

/* The type of the device */
enum ctx_device {
	DEVICE_ERROR		= -1,
	UNKNOWN			= 0,
	CONNECTX 		= 1,
	CONNECTX2 		= 2,
	CONNECTX3 		= 3,
	CONNECTIB 		= 4,
	LEGACY 			= 5,
	CHELSIO_T4 		= 6,
	CHELSIO_T5 		= 7,
	CONNECTX3_PRO		= 8,
	SKYHAWK			= 9,
	CONNECTX4		= 10,
	CONNECTX4LX		= 11
};


/*Accelerated verbs */
enum verbs_intf {
	NORMAL_INTF,
	ACCL_INTF,
};

// struct cpu_util_data {
	// int enable;
	// long long ustat[2];
	// long long idle[2];
// };

struct pingpong_context {
	struct rdma_event_channel		*cm_channel;
	struct rdma_cm_id			*cm_id_control;
	struct rdma_cm_id			*cm_id;
	struct ibv_context			*context;
	struct ibv_comp_channel			*channel;
	struct ibv_pd				*pd;
	struct ibv_mr				**mr;
	struct ibv_cq				*send_cq;
	struct ibv_cq				*recv_cq;
	uint64_t					**buf;
	struct ibv_ah				**ah;
	struct ibv_qp				**qp;
	struct ibv_srq				*srq;
	struct ibv_sge				*sge_list;
	struct ibv_sge				*recv_sge_list;
	struct ibv_send_wr			*wr;
	struct ibv_recv_wr			*rwr;
	uint64_t				size;
	uint64_t				*my_addr;
	uint64_t				*rx_buffer_addr;
	uint64_t				*rem_addr;
	uint64_t				buff_size;
	int					tx_depth;
	uint64_t				*scnt;
	uint64_t				*ccnt;
	int					is_contig_supported;
	uint32_t                                *ctrl_buf;
	uint32_t                                *credit_buf;
	struct ibv_mr                           *credit_mr;
	struct ibv_sge                          *ctrl_sge_list;
	struct ibv_send_wr                      *ctrl_wr;
	int                                     send_rcredit;
	int                                     credit_cnt;
	int					cache_line_size;
	int					cycle_buffer;
	
    drv_exp_post_send_func			exp_post_send_func_pointer;
    drv_post_send_func			post_send_func_pointer;
	drv_poll_cq_func			poll_cq_func_pointer;
	struct ibv_exp_dct			**dct;
	struct ibv_exp_send_wr			*exp_wr;
	

};

 struct pingpong_dest {
	int 				lid;
	int 				out_reads;
	int 				qpn;
	int 				psn;
	unsigned			rkey;
	unsigned long long		vaddr;
	union ibv_gid			gid;
	unsigned			srqn;
	int				gid_index;
 };
 
 struct perftest_parameters {

	int				port;
	char				*ib_devname;
	char				*servername;
	uint8_t				ib_port;
	uint8_t				ib_port2;
	int				mtu;
	enum ibv_mtu			curr_mtu;
	uint64_t			size;
	// uint64_t			dct_key;
	int				iters;
	uint64_t			iters_per_port[2];
	uint64_t			*port_by_qp;
	int				tx_depth;
	uint8_t				qp_timeout;
	uint8_t				sl;
	int				gid_index;
	int				gid_index2;
	int				use_gid_user;
	// uint8_t				source_mac[6];
	// uint8_t				dest_mac[6];
	// int				is_source_mac;
	// int				is_dest_mac;
	// uint32_t			server_ip;
	// uint32_t			client_ip;
	// int				is_server_ip;
	// int				is_client_ip;
	// int				server_port;
	// int				client_port;
	// int				tcp;
	// int				is_server_port;
	// int				is_client_port;
	// uint16_t			ethertype;
	// int				is_ethertype;
	// int				cpu_freq_f;
	int				connection_type;
	int				num_of_qps;
	int				use_event;
	int 				inline_size;
	// int				inline_recv_size;
	int				out_reads;
	int				rx_depth;
	// int				duplex;
	// int				noPeak;
	int				cq_mod;
	int 				spec;
	int 				dualport;
	int 				post_list;
	int				duration;
	int 				use_srq;
	int				use_xrc;
	int				use_rss;
	int				srq_exists;
	int				tos;
	int				margin;
	int 				is_bw_limit_passed;
	int 				is_msgrate_limit_passed;
	int 				is_limit_bw;
	int 				is_limit_msgrate;
	float				limit_bw;
	float				limit_msgrate;
	uint32_t			rem_ud_qpn;
	uint32_t			rem_ud_qkey;
	uint8_t				link_type;
	uint8_t				link_type2;
	MachineType			machine;
	PrintDataSide			side;
	VerbType			verb;
	TestType			tst;
	AtomicType			atomicType;
	TestMethod			test_type;
	// DurationStates			state;
	int				sockfd;
	int				use_mcg;
	// int 				use_rdma_cm;
	// int				is_reversed;
	// int				work_rdma_cm;
	// char				*user_mgid;
	int				buff_size;
	int             		pkey_index;
	int				raw_qos;
	// int				use_cuda;
	// int				use_rocm;
	// unsigned long			hsa_agent_index;
	// unsigned long			hsa_pool_index;
	enum ibv_transport_type 	transport_type;
	// int 				mac_fwd;
	int report_both; /* in bidirectional tests, report tx and rx separately */
	/* results limits */
	float 				min_bw_limit;
	float 				min_msgRate_limit;
	/* Rate Limiter */
	int 				is_rate_limiting;
	int 				rate_limit;
	int 				burst_size;
	enum 				rate_limiter_units rate_units;

	enum verbosity_level 		output;
	// int 				cpu_util;
	// struct cpu_util_data 		cpu_util_data; 
	int 				latency_gap;
	int 				retry_count;
	int 				dont_xchg_versions;
	int 				use_exp;
	int 				ipv6;
	int 				report_per_port;
	int 				use_odp;
	int				use_promiscuous;
	int				check_alive_exited;
	int				raw_mcast;
	int				masked_atomics;
	int				cycle_buffer;
	int				cache_line_size;
	// enum verbs_intf			verb_type;
	int				is_exp_cq;
	int				is_exp_qp;
	int				use_res_domain;
	int				mr_per_qp;
	uint16_t			dlid;
};
 
 struct perftest_comm {
	struct pingpong_context    *rdma_ctx;
	struct perftest_parameters *rdma_params;
};


static const char *connStr[] = {"RC","UC","UD","RawEth","XRC","DC"};
// static const char *testsStr[] = {"Send","RDMA_Write","RDMA_Read","Atomic"};
static const char *portStates[] = {"Nop","Down","Init","Armed","","Active Defer"};
// static const char *qp_state[] = {"OFF","ON"};
// static const char *exchange_state[] = {"Ethernet","rdma_cm"};
static const char *atomicTypesStr[] = {"CMP_AND_SWAP","FETCH_AND_ADD"};


// static enum ibv_exp_wr_opcode exp_opcode_verbs_array[] = {IBV_EXP_WR_SEND,IBV_EXP_WR_RDMA_WRITE,IBV_EXP_WR_RDMA_READ};
// static enum ibv_exp_wr_opcode exp_opcode_atomic_array[] = {IBV_EXP_WR_ATOMIC_CMP_AND_SWP,IBV_EXP_WR_ATOMIC_FETCH_AND_ADD};
static enum ibv_wr_opcode opcode_verbs_array[] = {IBV_WR_SEND,IBV_WR_RDMA_WRITE,IBV_WR_RDMA_READ};
static enum ibv_wr_opcode opcode_atomic_array[] = {IBV_WR_ATOMIC_CMP_AND_SWP,IBV_WR_ATOMIC_FETCH_AND_ADD};



struct perftest_parameters* duration_param;

static const char *sideArray[]  = {"local", "remote"};
static const char *gidArray[]   = {"GID"  , "MGID"};

int parser(struct perftest_parameters *user_param,char *argv[], int argc);
struct ibv_device* ctx_find_dev(const char *ib_devname);
int check_link(struct ibv_context *context,struct perftest_parameters *user_param);
int create_comm_struct(struct perftest_comm *comm,struct perftest_parameters *user_param);

//Parse related
static void init_perftest_params(struct perftest_parameters *user_param);
// static void usage(const char *argv0, VerbType verb, TestType tst, int connection_type);
static void force_dependecies(struct perftest_parameters *user_param);

//System related
//get
static int get_cache_line_size();
enum ctx_device ib_dev_name(struct ibv_context *context);
uint16_t ctx_get_local_lid(struct ibv_context *context,int port);

//set
static void change_conn_type(int *cptr,VerbType verb,const char *optarg);
static uint8_t set_link_layer(struct ibv_context *context,uint8_t ib_port);
static void ctx_set_max_inline(struct ibv_context *context,struct perftest_parameters *user_param);
static int ctx_set_out_reads(struct ibv_context *context,int num_user_reads);
enum ibv_mtu set_mtu(struct ibv_context *context,uint8_t ib_port,int user_mtu);

//check
int check_if_valid_udp_port(int udp_port);
static int ctx_chk_pkey_index(struct ibv_context *context,int pkey_idx);

//parse
int parse_ip_from_str(char *ip, u_int32_t *addr);
// static int parse_mac_from_str(char *mac, u_int8_t *addr);
// static int parse_ethertype_from_str(char *ether_str, uint16_t *ethertype_val);
const char *link_layer_str(uint8_t link_layer);
static inline int ipv6_addr_v4mapped(const struct in6_addr *a);

//IB related
void alloc_ctx(struct pingpong_context *ctx,struct perftest_parameters *user_param);
int ctx_init(struct pingpong_context *ctx, struct perftest_parameters *user_param);
static void get_verbs_pointers(struct pingpong_context *ctx);
static int check_for_contig_pages_support(struct ibv_context *context);
int create_mr(struct pingpong_context *ctx, struct perftest_parameters *user_param);
int create_single_mr(struct pingpong_context *ctx, struct perftest_parameters *user_param, int qp_index);
int create_cqs(struct pingpong_context *ctx, struct perftest_parameters *user_param);
int create_reg_cqs(struct pingpong_context *ctx, struct perftest_parameters *user_param, int tx_buffer_depth, int need_recv_cq);
int create_qp_main(struct pingpong_context *ctx, struct perftest_parameters *user_param, int i, int num_of_qps);
int create_reg_qp_main(struct pingpong_context *ctx, struct perftest_parameters *user_param, int i, int num_of_qps);
struct ibv_qp* ctx_qp_create(struct pingpong_context *ctx, struct perftest_parameters *user_param);
int modify_qp_to_init(struct pingpong_context *ctx, struct perftest_parameters *user_param, int qp_index, int num_of_qps);
int ctx_modify_qp_to_init(struct ibv_qp *qp,struct perftest_parameters *user_param, uint64_t init_flag);
int ctx_connect(struct pingpong_context *ctx, struct pingpong_dest *dest, struct perftest_parameters *user_param, struct pingpong_dest *my_dest);
static int ctx_modify_qp_to_rtr(struct ibv_qp *qp, struct ibv_qp_attr *attr, struct perftest_parameters *user_param, struct pingpong_dest *dest, struct pingpong_dest *my_dest, int qpindex);
static int ctx_modify_qp_to_rts(struct ibv_qp *qp, struct ibv_qp_attr *attr, struct perftest_parameters *user_param, struct pingpong_dest *dest, struct pingpong_dest *my_dest);
void ctx_set_send_wqes(struct pingpong_context *ctx, struct perftest_parameters *user_param, struct pingpong_dest *rem_dest);
void ctx_set_send_reg_wqes(struct pingpong_context *ctx, struct perftest_parameters *user_param, struct pingpong_dest *rem_dest);
int ctx_set_recv_wqes(struct pingpong_context *ctx,struct perftest_parameters *user_param);
int run_iter_lat_atomic(struct pingpong_context *ctx,struct perftest_parameters *user_param);
int run_iter_lat_write(struct pingpong_context *ctx,struct perftest_parameters *user_param);
int run_iter_lat_send(struct pingpong_context *ctx,struct perftest_parameters *user_param);
int ctx_close_connection(struct perftest_comm *comm, struct pingpong_dest *my_dest, struct pingpong_dest *rem_dest);
 
//ethernet related
int check_mtu(struct ibv_context *context,struct perftest_parameters *user_param, struct perftest_comm *user_comm);
int establish_connection(struct perftest_comm *comm);
static int ethernet_client_connect(struct perftest_comm *comm);
static int ethernet_server_connect(struct perftest_comm *comm);
int check_add_port(char **service, int port, const char *servername, struct addrinfo *hints, struct addrinfo **res);
int set_up_connection(struct pingpong_context *ctx, struct perftest_parameters *user_param, struct pingpong_dest *my_dest);
void ctx_print_pingpong_data(struct pingpong_dest *element, struct perftest_comm *comm);
int ctx_hand_shake(struct perftest_comm *comm, struct pingpong_dest *my_dest, struct pingpong_dest *rem_dest);
static int ethernet_read_keys(struct pingpong_dest *rem_dest, struct perftest_comm *comm);
static int ethernet_write_keys(struct pingpong_dest *my_dest, struct perftest_comm *comm);





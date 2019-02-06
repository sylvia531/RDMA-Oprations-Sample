#include "rdma_header.h"
/******************************************************************************
 * Function: main
 *
 * Input
 * argc number of items in argv
 * argv command line parameters
 *
 * Output
 * none
 *
 * Returns
 * 0 on success, 1 on failure
 *
 * Description
 * Main program code
 ******************************************************************************/
int main (int argc, char *argv[]){
    
	int				ret_parser, i;
	struct pingpong_context		ctx;
	struct pingpong_dest		*my_dest  = NULL;
	struct pingpong_dest		*rem_dest = NULL;
	struct ibv_device		*ib_dev;
	struct perftest_parameters	user_param;
	struct perftest_comm		user_comm;
	
	
	/* init default values to user's parameters */
	memset(&ctx,0,sizeof(struct pingpong_context));
	memset(&user_param, 0, sizeof(struct perftest_parameters));
	memset(&user_comm,0,sizeof(struct perftest_comm));
	
	// user_param.verb    = ATOMIC;
	// user_param.verb    = WRITE;
	user_param.verb    = SEND;
	user_param.tst     = LAT;
	
	
	ret_parser = parser(&user_param,argv,argc);
	if (ret_parser) {
		fprintf(stderr," Parser function exited with Error\n");
		return 1;
	}
	
	
	/* Finding the IB device selected (or defalut if no selected). */
	ib_dev = ctx_find_dev(user_param.ib_devname);
	if (!ib_dev) {
		fprintf(stderr," Unable to find the Infiniband/RoCE device\n");
		return FAILURE;
	}
	
	/* Getting the relevant context from the device */
	ctx.context = ibv_open_device(ib_dev);
	if (!ctx.context) {
		fprintf(stderr, " Couldn't get context for the device\n");
		return 1;
	}
	
	
	/* See if MTU and link type are valid and supported. */
	if (check_mtu(ctx.context,&user_param, &user_comm)) {
		fprintf(stderr, " Couldn't get context for the device\n");
		return FAILURE;
		
	}
	
	
	/* See if MTU and link type are valid and supported. */
	if (check_link(ctx.context,&user_param)) {
		fprintf(stderr, " Couldn't get context for the device\n");
		return FAILURE;
	}
	
	
	
	
	
	
	/*IB initial*/
	ALLOCATE(my_dest , struct pingpong_dest , user_param.num_of_qps);
	memset(my_dest, 0, sizeof(struct pingpong_dest)*user_param.num_of_qps);
	ALLOCATE(rem_dest , struct pingpong_dest , user_param.num_of_qps);
	memset(rem_dest, 0, sizeof(struct pingpong_dest)*user_param.num_of_qps);
	
	/* Allocating arrays needed for the test. */
	alloc_ctx(&ctx,&user_param);
	
	if (ctx_init(&ctx,&user_param)) {
		fprintf(stderr, " Couldn't create IB resources\n");
		return FAILURE;
	}
	
	
	
	
	
	
	

	/* copy the relevant user parameters to the comm struct + creating rdma_cm resources. */
	if (create_comm_struct(&user_comm,&user_param)) {
		fprintf(stderr," Unable to create RDMA_CM resources\n");
		return 1;
	}
	
	
	if (user_param.output == FULL_VERBOSITY && user_param.machine == SERVER) {
		printf("\n************************************\n");
		printf("* Waiting for client to connect... *\n");
		printf("************************************\n");
	}
	
	/* Initialize the connection and print the local data. */
	if (establish_connection(&user_comm)) {
		fprintf(stderr," Unable to init the socket connection\n");
		return FAILURE;
	}
	
	
	
	/* Set up the Connection. */
	if (set_up_connection(&ctx,&user_param,my_dest)) {
		fprintf(stderr," Unable to set up socket connection\n");
		return 1;
	}
	
	for (i=0; i < user_param.num_of_qps; i++){
		ctx_print_pingpong_data(&my_dest[i],&user_comm);
	}
	
	
	
	if (ctx_hand_shake(&user_comm,my_dest,rem_dest)) {
		fprintf(stderr,"Failed to exchange data between server and clients\n");
		return 1;
	}
	
	user_comm.rdma_params->side = REMOTE;
	
	for (i=0; i < user_param.num_of_qps; i++) {

		/* shaking hands and gather the other side info. */
		if (ctx_hand_shake(&user_comm,&my_dest[i],&rem_dest[i])) {
			fprintf(stderr,"Failed to exchange data between server and clients\n");
			return 1;
		}

		ctx_print_pingpong_data(&rem_dest[i],&user_comm);
	}
	
	if (ctx_connect(&ctx,rem_dest,&user_param,my_dest)) {
		fprintf(stderr," Unable to Connect the HCA's through the link\n");
		return 1;
	}
	
	/* An additional handshake is required after moving qp to RTR. */
	if (ctx_hand_shake(&user_comm,my_dest,rem_dest)) {
		fprintf(stderr,"Failed to exchange data between server and clients\n");
		return 1;
	}
	

	printf("local address %p, value: %lu\n", &(ctx.buf[0][0]), ctx.buf[0][0]);
	
	/* Only Client post read request. */
	// if (user_param.machine == SERVER) {
		
		// sleep(2);
		// printf("%lu\n", *ctx.buf[0]);
		// if (ctx_close_connection(&user_comm,my_dest,rem_dest)) {
			// fprintf(stderr,"Failed to close connection between server and client\n");
			// return 1;
		// }
		// return 0;
	// }
	
	ctx.buf[0][0] = 9;
	ctx_set_send_wqes(&ctx,&user_param,rem_dest);
	
	
	if(user_param.verb == ATOMIC){
		if(run_iter_lat_atomic(&ctx,&user_param)){
			fprintf(stderr,"Test exited with Error\n");
			return FAILURE;
		}
	}
	else if(user_param.verb == WRITE){
		if(run_iter_lat_write(&ctx,&user_param)) {
			fprintf(stderr,"Test exited with Error\n");
			return FAILURE;
		}
	}
	else if(user_param.verb == SEND){
		//Post recevie recv_wqes fo current message size
		printf("This is a send\n");
		if (ctx_set_recv_wqes(&ctx,&user_param)) {
			fprintf(stderr," Failed to post receive recv_wqes\n");
			return 1;
		}


		if (ctx_hand_shake(&user_comm,my_dest,rem_dest)) {
			fprintf(stderr,"Failed to exchange data between server and clients\n");
			return 1;
		}
		
		printf("Finish hand shake\n");

		if(run_iter_lat_send(&ctx, &user_param)){
			fprintf(stderr,"Test exited with Error\n");
			return FAILURE;
		}
	}
	
	
	
	if (ctx_close_connection(&user_comm,my_dest,rem_dest)) {
		fprintf(stderr,"Failed to close connection between server and client\n");
		return 1;
	}
	



	
	return 0;
}




int parser(struct perftest_parameters *user_param,char *argv[], int argc){
	int c,size_len;
	int size_factor = 1;
	static int run_inf_flag = 0;
	static int report_fmt_flag = 0;
	static int srq_flag = 0;
	static int report_both_flag = 0;
	static int is_reversed_flag = 0;
	static int pkey_flag = 0;
	// static int inline_recv_flag = 0;
	// static int tcp_flag = 0;
	static int burst_size_flag = 0;
	static int rate_limit_flag = 0;
	static int rate_units_flag = 0;
	static int verbosity_output_flag = 0;
	// static int cpu_util_flag = 0;
	static int latency_gap_flag = 0;
	static int retry_count_flag = 0;
	static int dont_xchg_versions_flag = 0;
	static int use_exp_flag = 0;
	// static int use_cuda_flag = 0;
	// static int use_rocm_flag = 0;
	static int ipv6_flag = 0;
	static int report_per_port_flag = 0;
	static int odp_flag = 0;
	static int use_promiscuous_flag = 0;
	static int raw_mcast_flag = 0;
	static int verb_type_flag = 0;
	static int use_res_domain_flag = 0;
	static int mr_per_qp_flag = 0;
	static int dlid_flag = 0;
	
	init_perftest_params(user_param);
	
	while (1) {
		static const struct option long_options[] = {
			{ .name = "port",		.has_arg = 1, .val = 'p' },
			{ .name = "ib-dev",		.has_arg = 1, .val = 'd' },
			{ .name = "ib-port",		.has_arg = 1, .val = 'i' },
			{ .name = "mtu",		.has_arg = 1, .val = 'm' },
			{ .name = "size",		.has_arg = 1, .val = 's' },
			{ .name = "iters",		.has_arg = 1, .val = 'n' },
			{ .name = "tx-depth",		.has_arg = 1, .val = 't' },
			{ .name = "qp-timeout",		.has_arg = 1, .val = 'u' },
			{ .name = "sl",			.has_arg = 1, .val = 'S' },
			{ .name = "gid-index",		.has_arg = 1, .val = 'x' },
			{ .name = "all",		.has_arg = 0, .val = 'a' },
			{ .name = "CPU-freq",		.has_arg = 0, .val = 'F' },
			{ .name = "connection",		.has_arg = 1, .val = 'c' },
			{ .name = "qp",			.has_arg = 1, .val = 'q' },
			{ .name = "events",		.has_arg = 0, .val = 'e' },
			{ .name = "inline_size",	.has_arg = 1, .val = 'I' },
			{ .name = "outs",		.has_arg = 1, .val = 'o' },
			{ .name = "mcg",		.has_arg = 0, .val = 'g' },
			{ .name = "comm_rdma_cm",	.has_arg = 0, .val = 'z' },
			{ .name = "rdma_cm",		.has_arg = 0, .val = 'R' },
			{ .name = "tos",		.has_arg = 1, .val = 'T' },
			{ .name = "help",		.has_arg = 0, .val = 'h' },
			{ .name = "MGID",		.has_arg = 1, .val = 'M' },
			{ .name = "rx-depth",		.has_arg = 1, .val = 'r' },
			{ .name = "bidirectional",	.has_arg = 0, .val = 'b' },
			{ .name = "cq-mod",		.has_arg = 1, .val = 'Q' },
			// { .name = "noPeak",		.has_arg = 0, .val = 'N' },
			// { .name = "version",		.has_arg = 0, .val = 'V' },
			// { .name = "report-cycles",	.has_arg = 0, .val = 'C' },
			{ .name = "report-histogrm",	.has_arg = 0, .val = 'H' },
			{ .name = "report-unsorted",	.has_arg = 0, .val = 'U' },
			{ .name = "atomic_type",	.has_arg = 1, .val = 'A' },
			{ .name = "dualport",		.has_arg = 0, .val = 'O' },
			{ .name = "post_list",		.has_arg = 1, .val = 'l' },
			{ .name = "duration",		.has_arg = 1, .val = 'D' },
			{ .name = "margin",		.has_arg = 1, .val = 'f' },
			{ .name = "source_mac",		.has_arg = 1, .val = 'B' },
			{ .name = "dest_mac",		.has_arg = 1, .val = 'E' },
			{ .name = "dest_ip",		.has_arg = 1, .val = 'J' },
			{ .name = "source_ip",		.has_arg = 1, .val = 'j' },
			{ .name = "dest_port",		.has_arg = 1, .val = 'K' },
			{ .name = "source_port",	.has_arg = 1, .val = 'k' },
			{ .name = "ethertype",		.has_arg = 1, .val = 'Y' },
			{ .name = "limit_bw",		.has_arg = 1, .val = 'w' },
			{ .name = "limit_msgrate",	.has_arg = 1, .val = 'y' },
			{ .name = "server",		.has_arg = 0, .val = 'Z' },
			{ .name = "client",		.has_arg = 0, .val = 'P' },
			// { .name = "mac_fwd",		.has_arg = 0, .val = 'v' },
			{ .name = "use_rss",		.has_arg = 0, .val = 'G' },
			{ .name = "run_infinitely",	.has_arg = 0, .flag = &run_inf_flag, .val = 1 },
			{ .name = "report_gbits",	.has_arg = 0, .flag = &report_fmt_flag, .val = 1},
			{ .name = "use-srq",		.has_arg = 0, .flag = &srq_flag, .val = 1},
			{ .name = "report-both",	.has_arg = 0, .flag = &report_both_flag, .val = 1},
			{ .name = "reversed",		.has_arg = 0, .flag = &is_reversed_flag, .val = 1},
			{ .name = "pkey_index",		.has_arg = 1, .flag = &pkey_flag, .val = 1},
			// { .name = "inline_recv",	.has_arg = 1, .flag = &inline_recv_flag, .val = 1},
			// { .name = "tcp",		.has_arg = 0, .flag = &tcp_flag, .val = 1},
			{ .name = "burst_size",		.has_arg = 1, .flag = &burst_size_flag, .val = 1},
			{ .name = "rate_limit",		.has_arg = 1, .flag = &rate_limit_flag, .val = 1},
			{ .name = "rate_units",		.has_arg = 1, .flag = &rate_units_flag, .val = 1},
			{ .name = "output",		.has_arg = 1, .flag = &verbosity_output_flag, .val = 1},
			// { .name = "cpu_util",		.has_arg = 0, .flag = &cpu_util_flag, .val = 1},
			{ .name = "latency_gap",	.has_arg = 1, .flag = &latency_gap_flag, .val = 1},
			{ .name = "retry_count",	.has_arg = 1, .flag = &retry_count_flag, .val = 1},
			{ .name = "dont_xchg_versions",	.has_arg = 0, .flag = &dont_xchg_versions_flag, .val = 1},
			// { .name = "use_cuda",		.has_arg = 0, .flag = &use_cuda_flag, .val = 1},
			{ .name = "ipv6",		.has_arg = 0, .flag = &ipv6_flag, .val = 1},
			{ .name = "report-per-port",	.has_arg = 0, .flag = &report_per_port_flag, .val = 1},
			{ .name = "odp",		.has_arg = 0, .flag = &odp_flag, .val = 1},
			{ .name = "promiscuous",	.has_arg = 0, .flag = &use_promiscuous_flag, .val = 1},
			{ .name = "raw_mcast",		.has_arg = 0, .flag = &raw_mcast_flag, .val = 1},
			{ .name = "use_exp",		.has_arg = 0, .flag = &use_exp_flag, .val = 1},
			{ .name = "mr_per_qp",		.has_arg = 0, .flag = &mr_per_qp_flag, .val = 1},
			{ .name = "dlid",		.has_arg = 1, .flag = &dlid_flag, .val = 1},
			{ 0 }
		};
		
		printf("use_exp_flag= %d\n", use_exp_flag);
		c = getopt_long(argc,argv,"w:y:p:d:i:m:s:n:t:u:S:x:c:q:I:o:M:r:Q:A:l:D:f:B:T:E:J:j:K:k:aFegzRvhbNVCHUOZP",long_options,NULL);
		
		if (c == -1)
			break;

		switch (c) {

			case 'p': user_param->port = strtol(optarg, NULL, 0); break;
			case 'd': GET_STRING(user_param->ib_devname,strdupa(optarg)); break;
			case 'i': CHECK_VALUE(user_param->ib_port,uint8_t,MIN_IB_PORT,MAX_IB_PORT,"IB Port"); break;
			case 'm': user_param->mtu  = strtol(optarg, NULL, 0); break;
			case 'n': CHECK_VALUE(user_param->iters,int,MIN_ITER,MAX_ITER,"Iteration num"); break;
			case 't': CHECK_VALUE(user_param->tx_depth,int,MIN_TX,MAX_TX,"Tx depth"); break;
			case 'T': CHECK_VALUE(user_param->tos,int,MIN_TOS,MAX_TOS,"TOS"); break;
			case 'u': user_param->qp_timeout = (uint8_t)strtol(optarg, NULL, 0); break;
			case 'S': user_param->sl = (uint8_t)strtol(optarg, NULL, 0);
				  if (user_param->sl > MAX_SL) {
					  fprintf(stderr," Only %d Service levels\n",MAX_SL);
					  return 1;
				  }
				  break;
			case 'x': CHECK_VALUE(user_param->gid_index,uint8_t,MIN_GID_IX,MAX_GID_IX,"Gid index");
				  user_param->use_gid_user = 1; break;
			case 'c': change_conn_type(&user_param->connection_type,user_param->verb,optarg); break;
			case 'q':
				  if (user_param->tst != BW) {
					  fprintf(stderr," Multiple QPs only available on bw tests\n");
					  return 1;
				  }
				  CHECK_VALUE(user_param->num_of_qps,int,MIN_QP_NUM,MAX_QP_NUM,"num of Qps");
				  break;
			case 'I': CHECK_VALUE(user_param->inline_size,int,0,MAX_INLINE,"Max inline");
				  if (user_param->verb == READ || user_param->verb ==ATOMIC) {
					  fprintf(stderr," Inline feature not available on READ/Atomic verbs\n");
					  return 1;
				  } break;
			case 'o': user_param->out_reads = strtol(optarg, NULL, 0);
				  if (user_param->verb != READ && user_param->verb != ATOMIC) {
					  fprintf(stderr," Setting Outstanding reads only available on READ verb\n");
					  return 1;
				  } break;
			// case 'M': GET_STRING(user_param->user_mgid,strdupa(optarg)); break;
			case 'r': CHECK_VALUE(user_param->rx_depth,int,MIN_RX,MAX_RX," Rx depth");
				  if (user_param->verb != SEND && user_param->rx_depth > DEF_RX_RDMA) {
					  fprintf(stderr," On RDMA verbs rx depth can be only 1\n");
					  return 1;
				  } break;
			case 'Q': CHECK_VALUE(user_param->cq_mod,int,MIN_CQ_MOD,MAX_CQ_MOD,"CQ moderation"); break;
			case 'A':
				  if (user_param->verb != ATOMIC) {
					  fprintf(stderr," You are not running the atomic_lat/bw test!\n");
					  fprintf(stderr," To change the atomic action type, you must run one of the atomic tests\n");
					  return 1;
				  }

				  if (strcmp(atomicTypesStr[0],optarg)==0)
					  user_param->atomicType = CMP_AND_SWAP;

				  else if (strcmp(atomicTypesStr[1],optarg)==0)
					  user_param->atomicType = FETCH_AND_ADD;

				  else {
					  fprintf(stderr," Invalid Atomic type! please choose from {CMP_AND_SWAP,FETCH_AND_ADD}\n");
					  exit(1);
				  }
				  break;
			case 'l': user_param->post_list = strtol(optarg, NULL, 0); break;
			// case 'D': user_param->duration = strtol(optarg, NULL, 0);
				  // if (user_param->duration <= 0) {
					  // fprintf(stderr," Duration period must be greater than 0\n");
					  // return 1;
				  // }
				  // user_param->test_type = DURATION;
				  // break;
			case 'f': user_param->margin = strtol(optarg, NULL, 0);
				  if (user_param->margin < 0) {
					  fprintf(stderr," margin must be positive.\n");
					  return 1;
				  } break;
			case 'O':
				  user_param->ib_port  = DEF_IB_PORT;
				  user_param->ib_port2 = DEF_IB_PORT2;
				  user_param->dualport = ON;
				  break;
			// case 'F': user_param->cpu_freq_f = ON; break;
			// case 'h': usage(argv[0], user_param->verb, user_param->tst, user_param->connection_type);
				  // return HELP_EXIT;
			// case 'z': user_param->use_rdma_cm = ON; break;
			// case 'R': user_param->work_rdma_cm = ON; break;
			case 's': size_len = (int)strlen(optarg);
				  if (optarg[size_len-1] == 'K') {
					  optarg[size_len-1] = '\0';
					  size_factor = 1024;
				  }
				  if (optarg[size_len-1] == 'M') {
					  optarg[size_len-1] = '\0';
					  size_factor = 1024*1024;
				  }
				  user_param->size = (uint64_t)strtol(optarg, NULL, 0) * size_factor;
				  
				  if (user_param->size < 1 || user_param->size > (UINT_MAX / 2)) {
					  fprintf(stderr," Message Size should be between %d and %d\n",1,UINT_MAX/2);
					  return 1;
				  }
				  break;
			case 'e': user_param->use_event = ON;
				  if (user_param->verb == WRITE) {
					  fprintf(stderr," Events feature not available on WRITE verb\n");
					  return 1;
				  } break;
			// case 'b': user_param->duplex = ON;
				  // if (user_param->tst == LAT) {
					  // fprintf(stderr," Bidirectional is only available in BW test\n");
					  // return 1;
				  // } break;
			// case 'N': user_param->noPeak = ON;
				  // if (user_param->tst == LAT) {
					  // fprintf(stderr," NoPeak only valid for BW tests\n");
					  // return 1;
				  // } break;
			// case 'C':
				  // if (user_param->tst != LAT) {
					  // fprintf(stderr," Availible only on Latency tests\n");
					  // return 1;
				  // }
				  // break;
			case 'g': user_param->use_mcg = ON;
				  if (user_param->verb != SEND) {
					  fprintf(stderr," MultiCast feature only available on SEND verb\n");
					  return 1;
				  } break;
			case 'H':
				  if (user_param->tst != LAT) {
					  fprintf(stderr," Availible only on Latency tests\n");
					  return 1;
				  }
				  break;
			case 'U':
				  if (user_param->tst != LAT) {
					  fprintf(stderr," Availible only on Latency tests\n");
					  return 1;
				  }
				  break;
			// case 'B':
				  // user_param->is_source_mac = ON;
				  // if(parse_mac_from_str(optarg, user_param->source_mac))
					  // return FAILURE;
				  // break;
			// case 'E':
				  // user_param->is_dest_mac = ON;
				  // if(parse_mac_from_str(optarg, user_param->dest_mac))
					  // return FAILURE;
				  // break;
			// case 'J':
				  // user_param->is_server_ip = ON;
				  // if(1 != parse_ip_from_str(optarg, &(user_param->server_ip))) {
					  // fprintf(stderr," Invalid server IP address\n");
					  // return FAILURE;
				  // }
				  // break;
			// case 'j':
				  // user_param->is_client_ip = ON;
				  // if(1 != parse_ip_from_str(optarg, &(user_param->client_ip))) {
					  // fprintf(stderr," Invalid client IP address\n");
					  // return FAILURE;
				  // }
				  // break;
			// case 'K':
				  // user_param->is_server_port = ON;
				  // user_param->server_port = strtol(optarg, NULL, 0);
				  // if(OFF == check_if_valid_udp_port(user_param->server_port)) {
					  // fprintf(stderr," Invalid server UDP port\n");
					  // return FAILURE;
				  // }
				  // break;
			// case 'k':
				  // user_param->is_client_port = ON;
				  // user_param->client_port = strtol(optarg, NULL, 0);
				  // if(OFF == check_if_valid_udp_port(user_param->client_port)) {
					  // fprintf(stderr," Invalid client UDP port\n");
					  // return FAILURE;
				  // }
				  // break;
			// case 'Y':
				  // user_param->is_ethertype = ON;
				  // if (parse_ethertype_from_str(optarg, &user_param->ethertype)) {
					  // fprintf(stderr, " Invalid ethertype value\n");
					  // return FAILURE;
				  // }
				  // break;
			case 'w':
				  user_param->is_limit_bw = ON;
				  user_param->limit_bw = strtof(optarg,NULL);
				  if (user_param->limit_bw < 0) {
					  fprintf(stderr, " Invalid Minimum BW Limit\n");
					  return FAILURE;
				  }
				  break;
			case 'y':
				  user_param->is_limit_msgrate = ON;
				  user_param->limit_msgrate = strtof(optarg,NULL);
				  if (user_param->limit_msgrate < 0) {
					  fprintf(stderr, " Invalid Minimum msgRate Limit\n");
					  return FAILURE;
				  }
				  break;
			case 'P': user_param->machine = CLIENT; break;
			case 'Z': user_param->machine = SERVER; break;
			// case 'v': user_param->mac_fwd = ON; break;
			case 'G': user_param->use_rss = ON; break;
			case 0: /* required for long options to work. */
				  if (pkey_flag) {
					  user_param->pkey_index = strtol(optarg,NULL,0);
					  pkey_flag = 0;
				  }
				  // if (inline_recv_flag) {
					  // user_param->inline_recv_size = strtol(optarg,NULL,0);
					  // inline_recv_flag = 0;
				  // }
				  if (rate_limit_flag) {
					  user_param->is_rate_limiting = 1;
					  user_param->rate_limit = strtol(optarg,NULL,0);
					  if (user_param->rate_limit < 0) {
						  fprintf(stderr, " Rate limit must be non-negative\n");
						  return FAILURE;
					  }
					  rate_limit_flag = 0;
				  }
				  if (burst_size_flag) {
					  user_param->burst_size = strtol(optarg,NULL,0);
					  if (user_param->burst_size < 0) {
						  fprintf(stderr, " Burst size must be non-negative\n");
						  return FAILURE;
					  }
					  burst_size_flag = 0;
				  }
				  if (rate_units_flag) {
					  if (strcmp("M",optarg) == 0) {
						  user_param->rate_units = MEGA_BYTE_PS;
					  } else if (strcmp("g",optarg) == 0) {
						  user_param->rate_units = GIGA_BIT_PS;
					  } else if (strcmp("p",optarg) == 0) {
						  user_param->rate_units = PACKET_PS;
					  } else {
						  fprintf(stderr, " Invalid rate limit units. Please use M,g or p\n");
						  return FAILURE;
					  }
					  rate_units_flag = 0;
				  }
				  if (verbosity_output_flag) {
					  if (strcmp("bandwidth",optarg) == 0) {
						  user_param->output = OUTPUT_BW;
					  } else if (strcmp("message_rate",optarg) == 0) {
						  user_param->output = OUTPUT_MR;
					  } else if (strcmp("latency",optarg) == 0) {
						  user_param->output = OUTPUT_LAT;
					  } else {
						  fprintf(stderr, " Invalid verbosity level output flag. Please use bandwidth, latency, message_rate\n");
						  return FAILURE;
					  }
					  verbosity_output_flag = 0;
				  }
				  if (latency_gap_flag) {
					  user_param->latency_gap = strtol(optarg,NULL,0);
					  if (user_param->latency_gap < 0) {
						  fprintf(stderr, " Latency gap time must be non-negative\n");
						  return FAILURE;
					  }
					  latency_gap_flag = 0;
				  }
				  if (retry_count_flag) {
					  user_param->retry_count = strtol(optarg,NULL,0);
					  if (user_param->retry_count < 0) {
						  fprintf(stderr, " Retry Count value must be positive\n");
						  return FAILURE;
					  }
					  retry_count_flag = 0;
				  }
				  if (verb_type_flag) {
					  // if (strcmp("normal",optarg) == 0) {
						  // user_param->verb_type = NORMAL_INTF;
					  // } else if (strcmp("accl",optarg) == 0) {
						  // user_param->verb_type = ACCL_INTF;
					  // } else {
						  // fprintf(stderr, " Invalid verb type. Please choose normal/accl.\n");
						  // return FAILURE;
					  // }
					  verb_type_flag = 0;
				  }
				  if (dlid_flag) {
					  user_param->dlid = (uint16_t)strtol(optarg, NULL, 0);
					  dlid_flag = 0;
				  }
				  break;

			default:
				  fprintf(stderr," Invalid Command or flag.\n");
				  fprintf(stderr," Please check command line and run again.\n\n");
				  // usage(argv[0], user_param->verb, user_param->tst, user_param->connection_type);
				  return 1;
		}
	}
	
	// if (tcp_flag) {
		// user_param->tcp = 1;
	// }

	if (srq_flag) {
		user_param->use_srq = 1;
	}


	if (dont_xchg_versions_flag) {
		user_param->dont_xchg_versions = 1;
	}

	if (use_exp_flag) {
		user_param->use_exp = 1;
	}

	if (use_res_domain_flag) {
		user_param->use_res_domain = 1;
	}

	// if (use_cuda_flag) {
		// user_param->use_cuda = 1;
	// }

	// if (use_rocm_flag) {
		// user_param->use_rocm = 1;
	// }

	if (report_both_flag) {
		user_param->report_both = 1;
	}

	// if (is_reversed_flag) {
		// user_param->is_reversed = 1;
	// }

	// if (cpu_util_flag) {
		// user_param->cpu_util = 1;
	// }

	if (report_per_port_flag) {
		user_param->report_per_port = 1;
	}

	if (ipv6_flag) {
		user_param->ipv6 = 1;
	}

	if(odp_flag) {
		user_param->use_odp = 1;
	}

	if (use_promiscuous_flag) {
		user_param->use_promiscuous = 1;
	}

	if (raw_mcast_flag) {
		user_param->raw_mcast = 1;
	}

	if (mr_per_qp_flag) {
		user_param->mr_per_qp = 1;
	}

	if (optind == argc - 1) {
		GET_STRING(user_param->servername,strdupa(argv[optind]));

	} else if (optind < argc) {
		fprintf(stderr," Invalid Command line. Please check command rerun \n");
		return 1;
	}

	user_param->machine = user_param->servername ? CLIENT : SERVER;

	/* fan-in addition */
	// if (user_param->is_reversed) {
		// if (user_param->machine == SERVER)
			// user_param->machine = CLIENT;
		// else
			// user_param->machine = SERVER;
	// }
	

	force_dependecies(user_param);
	
	return 0;
}


struct ibv_device* ctx_find_dev(const char *ib_devname){
	int num_of_device;
	struct ibv_device **dev_list;
	struct ibv_device *ib_dev = NULL;

	dev_list = ibv_get_device_list(&num_of_device);

	if (num_of_device <= 0) {
		fprintf(stderr," Did not detect devices \n");
		fprintf(stderr," If device exists, check if driver is up\n");
		return NULL;
	}

	if (!ib_devname) {
		ib_dev = dev_list[0];
		if (!ib_dev) {
			fprintf(stderr, "No IB devices found\n");
			exit (1);
		}
	} else {
		for (; (ib_dev = *dev_list); ++dev_list)
			if (!strcmp(ibv_get_device_name(ib_dev), ib_devname))
				break;
		if (!ib_dev)
			fprintf(stderr, "IB device %s not found\n", ib_devname);
	}
	return ib_dev;
}


int check_link(struct ibv_context *context,struct perftest_parameters *user_param){
	user_param->transport_type = context->device->transport_type;
	user_param->link_type = set_link_layer(context,user_param->ib_port);

	if (user_param->link_type == LINK_FAILURE) {
		fprintf(stderr, " Couldn't set the link layer\n");
		return FAILURE;
	}

	if (user_param->link_type == IBV_LINK_LAYER_ETHERNET &&  user_param->gid_index == -1) {
		user_param->gid_index = 0;
	}


	/* in case of dual-port mode */
	if (user_param->dualport==ON) {
		user_param->link_type2 = set_link_layer(context,user_param->ib_port2);
		if (user_param->link_type2 == IBV_LINK_LAYER_ETHERNET &&  user_param->gid_index2 == -1) {
			user_param->gid_index2 = 1;
		}
		if (user_param->link_type2 == LINK_FAILURE) {
			fprintf(stderr, " Couldn't set the link layer\n");
			return FAILURE;
		}
	}

	/* Compute Max inline size with pre found statistics values */
	ctx_set_max_inline(context,user_param);

	if (user_param->verb == READ || user_param->verb == ATOMIC){
		user_param->out_reads = ctx_set_out_reads(context,user_param->out_reads);
	}
	else{
		user_param->out_reads = 1;
	}


	if (!user_param->ib_devname)
		GET_STRING(user_param->ib_devname,ibv_get_device_name(context->device))

	if (user_param->pkey_index > 0)
		user_param->pkey_index = ctx_chk_pkey_index(context, user_param->pkey_index);

	return SUCCESS;
}


int create_comm_struct(struct perftest_comm *comm,struct perftest_parameters *user_param){
	ALLOCATE(comm->rdma_params, struct perftest_parameters, 1);
	memset(comm->rdma_params, 0, sizeof(struct perftest_parameters));

	comm->rdma_params->port		   	= user_param->port;
	comm->rdma_params->sockfd      		= -1;
	comm->rdma_params->gid_index   		= user_param->gid_index;
	comm->rdma_params->gid_index2 		= user_param->gid_index2;
	// comm->rdma_params->use_rdma_cm 		= user_param->use_rdma_cm;
	comm->rdma_params->servername  		= user_param->servername;
	comm->rdma_params->machine 	   	= user_param->machine;
	comm->rdma_params->side		   	= LOCAL;
	comm->rdma_params->verb		   	= user_param->verb;
	comm->rdma_params->use_mcg	   	= user_param->use_mcg;
	// comm->rdma_params->duplex	   	= user_param->duplex;
	comm->rdma_params->tos         		= DEF_TOS;
	comm->rdma_params->use_xrc	   	= user_param->use_xrc;
	comm->rdma_params->connection_type	= user_param->connection_type;
	comm->rdma_params->output      		= user_param->output;
	comm->rdma_params->report_per_port 	= user_param->report_per_port;
	comm->rdma_params->retry_count		= user_param->retry_count;
	comm->rdma_params->mr_per_qp		= user_param->mr_per_qp;
	comm->rdma_params->dlid			= user_param->dlid;

	return SUCCESS;
}










//Parse related

static void init_perftest_params(struct perftest_parameters *user_param){
	user_param->port		= DEF_PORT;
	user_param->ib_port		= DEF_IB_PORT;
	user_param->ib_port2		= DEF_IB_PORT2;
	user_param->size		=  DEF_SIZE_LAT;
	user_param->tx_depth		= DEF_TX_LAT;
	user_param->qp_timeout		= DEF_QP_TIME;
	// user_param->cpu_freq_f		= OFF;
	user_param->connection_type	= RC;
	user_param->use_event		= OFF;
	user_param->num_of_qps		= DEF_NUM_QPS;
	user_param->gid_index		= DEF_GID_INDEX;
	user_param->gid_index2		= DEF_GID_INDEX;
	user_param->use_gid_user	= 0;
	user_param->inline_size		= DEF_INLINE;
	user_param->use_mcg		= OFF;
	// user_param->use_rdma_cm		= OFF;
	// user_param->work_rdma_cm	= OFF;
	user_param->rx_depth		= user_param->verb == SEND ? DEF_RX_SEND : DEF_RX_RDMA;
	// user_param->duplex		= OFF;
	// user_param->noPeak		= OFF;
	user_param->cq_mod		= DEF_CQ_MOD;
	user_param->iters		= DEF_ITERS;
	user_param->dualport		= OFF;
	user_param->post_list		= 1;
	user_param->use_srq		= OFF;
	user_param->use_xrc		= OFF;
	user_param->use_rss		= OFF;
	user_param->srq_exists		= OFF;
	// user_param->duration		= DEF_DURATION;
	user_param->margin		= DEF_INIT_MARGIN;
	user_param->test_type		= ITERATIONS;
	// user_param->state		= START_STATE;
	user_param->tos			= DEF_TOS;
	// user_param->mac_fwd		= OFF;
	user_param->report_both		= OFF;
	// user_param->is_reversed		= OFF;
	user_param->is_limit_bw		= OFF;
	user_param->limit_bw		= 0;
	user_param->is_limit_msgrate	= OFF;
	user_param->limit_msgrate	= 0;
	user_param->pkey_index		= 0;
	user_param->raw_qos		= 0;
	// user_param->inline_recv_size	= 0;
	// user_param->tcp			= 0;
	user_param->is_rate_limiting	= 0;
	user_param->burst_size		= 0;
	user_param->rate_limit		= 0;
	user_param->rate_units		= MEGA_BYTE_PS;
	user_param->output		= -1;
	// user_param->use_cuda		= 0;
	// user_param->use_rocm		= 0;
	// user_param->hsa_agent_index	= 0;
	// user_param->hsa_pool_index	= 0;
	user_param->iters_per_port[0]	= 0;
	user_param->iters_per_port[1]	= 0;

	
	if (user_param->verb == ATOMIC) {
		user_param->atomicType	= FETCH_AND_ADD;
		user_param->size	= DEF_SIZE_ATOMIC;
		printf("In parse: user_param->size = %lu, uint64 = %lu\n", user_param->size, sizeof(uint64_t));
	}

	// user_param->cpu_util			= 0;
	// user_param->cpu_util_data.enable	= 0;
	user_param->retry_count			= DEF_RETRY_COUNT;
	user_param->dont_xchg_versions		= 0;
	user_param->use_exp			= 0;
	user_param->ipv6			= 0;
	user_param->report_per_port		= 0;
	user_param->use_odp			= 0;
	user_param->use_promiscuous		= 0;
	user_param->check_alive_exited		= 0;
	user_param->raw_mcast			= 0;
	user_param->masked_atomics		= 0;
	user_param->cache_line_size		= get_cache_line_size();
	user_param->cycle_buffer		= sysconf(_SC_PAGESIZE);

	if (user_param->cycle_buffer <= 0) {
		user_param->cycle_buffer = DEF_PAGE_SIZE;
	}

	// user_param->verb_type		= NORMAL_INTF;
	user_param->is_exp_cq		= 0;
	user_param->is_exp_qp		= 0;
	user_param->use_res_domain	= 0;
	user_param->mr_per_qp		= 0;
	user_param->dlid		= 0;
}

/*
static void usage(const char *argv0, VerbType verb, TestType tst, int connection_type){
	printf("Usage:\n");
	printf("  %s            start a server and wait for connection\n", argv0);
	printf("  %s <host>     connect to server at <host>\n", argv0);
	printf("\n");
	printf("Options:\n");

	if (verb != ATOMIC) {
		printf("  -a, --all ");
		printf(" Run sizes from 2 till 2^23\n");
	}

	if (verb == ATOMIC) {
		printf("  -A, --atomic_type=<type> ");
		printf(" type of atomic operation from {CMP_AND_SWAP,FETCH_AND_ADD} (default FETCH_AND_ADD)\n");
	}

	if (tst == BW) {
		printf("  -b, --bidirectional ");
		printf(" Measure bidirectional bandwidth (default unidirectional)\n");
	}

	if (verb == SEND) {
		printf("  -c, --connection=<RC/XRC/UC/UD/DC> ");
		printf(" Connection type RC/XRC/UC/UD/DC (default RC)\n");
	} else 	if (verb == WRITE) {
		printf("  -c, --connection=<RC/XRC/UC/DC> ");
		printf(" Connection type RC/XRC/UC/DC (default RC)\n");
	} else if (verb == READ || verb == ATOMIC) {
		printf("  -c, --connection=<RC/XRC/DC> ");
		printf(" Connection type RC/XRC/DC (default RC)\n");
	}

	if (tst == LAT) {
		printf("  -C, --report-cycles ");
		printf(" report times in cpu cycle units (default microseconds)\n");
	}

	printf("  -d, --ib-dev=<dev> ");
	printf(" Use IB device <dev> (default first device found)\n");

	printf("  -D, --duration ");
	printf(" Run test for a customized period of seconds.\n");

	if (verb != WRITE) {
		printf("  -e, --events ");
		printf(" Sleep on CQ events (default poll)\n");
	}

	printf("  -f, --margin ");
	printf(" measure results within margins. (default=2sec)\n");

	printf("  -F, --CPU-freq ");
	printf(" Do not show a warning even if cpufreq_ondemand module is loaded, and cpu-freq is not on max.\n");

	if (verb == SEND) {
		printf("  -g, --mcg ");
		printf(" Send messages to multicast group with 1 QP attached to it.\n");
	}

	printf("  -h, --help ");
	printf(" Show this help screen.\n");

	if (tst == LAT) {
		printf("  -H, --report-histogram ");
		printf(" Print out all results (default print summary only)\n");
	}

	printf("  -i, --ib-port=<port> ");
	printf(" Use port <port> of IB device (default %d)\n",DEF_IB_PORT);

	if (verb != READ && verb != ATOMIC) {
		printf("  -I, --inline_size=<size> ");
		printf(" Max size of message to be sent in inline\n");
	}

	if (tst == BW) {
		printf("  -l, --post_list=<list size>");
		printf(" Post list of WQEs of <list size> size (instead of single post)\n");
	}

	printf("  -m, --mtu=<mtu> ");
	printf(" Mtu size : 256 - 4096 (default port mtu)\n");

	if (verb == SEND) {
		printf("  -M, --MGID=<multicast_gid> ");
		printf(" In multicast, uses <multicast_gid> as the group MGID.\n");
	}

	printf("  -n, --iters=<iters> ");
	printf(" Number of exchanges (at least %d, default %d)\n", MIN_ITER, DEF_ITERS);

	if (tst == BW) {
		printf("  -N, --noPeak");
		printf(" Cancel peak-bw calculation (default with peak up to iters=20000)\n");
	}

	if (verb == READ || verb == ATOMIC) {
		printf("  -o, --outs=<num> ");
		printf(" num of outstanding read/atom(default max of device)\n");
	}

	if (tst == BW) {
		printf("  -O, --dualport ");
		printf(" Run test in dual-port mode.\n");
	}

	printf("  -p, --port=<port> ");
	printf(" Listen on/connect to port <port> (default %d)\n",DEF_PORT);

	if (tst == BW) {
		printf("  -q, --qp=<num of qp's>  Num of qp's(default %d)\n",DEF_NUM_QPS);
	}

	if (tst == BW) {
		printf("  -Q, --cq-mod ");
		printf(" Generate Cqe only after <--cq-mod> completion\n");
	}

	if (verb == SEND) {
		printf("  -r, --rx-depth=<dep> ");
		printf(" Rx queue size (default %d).",DEF_RX_SEND);
		printf(" If using srq, rx-depth controls max-wr size of the srq\n");
	}

	printf("  -R, --rdma_cm ");
	printf(" Connect QPs with rdma_cm and run test on those QPs\n");

	if (verb != ATOMIC) {
		printf("  -s, --size=<size> ");
		printf(" Size of message to exchange (default %d)\n", DEF_SIZE_LAT);
	}

	printf("  -S, --sl=<sl> ");
	printf(" SL (default %d)\n",DEF_SL);

	if (tst == BW) {
		printf("  -t, --tx-depth=<dep> ");
		printf(" Size of tx queue (default %d)\n",DEF_TX_LAT);
	}

	printf("  -T, --tos=<tos value> ");
	printf(" Set <tos_value> to RDMA-CM QPs. availible only with -R flag. values 0-256 (default off)\n");

	printf("  -u, --qp-timeout=<timeout> ");
	printf(" QP timeout, timeout value is 4 usec * 2 ^(timeout), default %d\n",DEF_QP_TIME);

	if (tst == LAT) {
		printf("  -U, --report-unsorted ");
		printf(" (implies -H) print out unsorted results (default sorted)\n");
	}

	printf("  -V, --version ");
	printf(" Display version number\n");

	if (tst == BW) {
		printf("  -w, --limit_bw ");
		printf(" Set verifier limit for bandwidth\n");
	}
	printf("  -x, --gid-index=<index> ");
	printf(" Test uses GID with GID index (Default : IB - no gid . ETH - 0)\n");

	if (tst == BW) {
		printf("  -y, --limit_msgrate ");
		printf(" Set verifier limit for Msg Rate\n");
	}

	printf("  -z, --com_rdma_cm ");
	printf(" Communicate with rdma_cm module to exchange data - use regular QPs\n");

	//Long flags
	putchar('\n');

	printf("      --cpu_util ");
	printf(" Show CPU Utilization in report, valid only in Duration mode \n");

	printf("      --dlid ");
	printf(" Set a Destination LID instead of getting it from the other side.\n");

	printf("      --dont_xchg_versions ");
	printf(" Do not exchange versions and MTU with other side \n");

	if (verb != WRITE) {
		printf("      --inline_recv=<size> ");
		printf(" Max size of message to be sent in inline receive\n");
	}

	printf("      --ipv6 ");
	printf(" Use IPv6 GID. Default is IPv4\n");

	if (tst == LAT) {
		printf("      --latency_gap=<delay_time> ");
		printf(" delay time between each post send\n");
	}

	printf("      --mmap=file ");
	printf(" Use an mmap'd file as the buffer for testing P2P transfers.\n");
	printf("      --mmap-offset=<offset> ");
	printf(" Use an mmap'd file as the buffer for testing P2P transfers.\n");

	if (tst == BW) {
		printf("      --mr_per_qp ");
		printf(" Create memory region for each qp.\n");
	}

	#ifdef HAVE_ODP
	printf("      --odp ");
	printf(" Use On Demand Paging instead of Memory Registration.\n");
	#endif

	printf("      --output=<units>");
	printf(" Set verbosity output level: bandwidth , message_rate, latency \n");

	printf("      --pkey_index=<pkey index> PKey index to use for QP\n");

	if ( tst == BW ) {
		printf("      --report-both ");
		printf(" Report RX & TX results separately on Bidirectinal BW tests\n");

		printf("      --report_gbits ");
		printf(" Report Max/Average BW of test in Gbit/sec (instead of MB/sec)\n");

		printf("      --report-per-port ");
		printf(" Report BW data on both ports when running Dualport and Duration mode\n");

		printf("      --reversed ");
		printf(" Reverse traffic direction - Server send to client\n");

		printf("      --run_infinitely ");
		printf(" Run test forever, print results every <duration> seconds\n");
	}

	printf("      --retry_count=<value> ");
	printf(" Set retry count value in rdma_cm mode\n");

	#ifdef HAVE_CUDA
	printf("      --use_cuda ");
	printf(" Use CUDA lib for GPU-Direct testing.\n");
	#endif

	#ifdef HAVE_ROCM
	printf("      --use_rocm=[agent,pool] ");
	printf(" Allocate HSA global memory pool for agent for PeerDirect testing");
	printf("(default: first found HSA agent and global pool (0-based))\n");
	#endif


	#ifdef HAVE_VERBS_EXP
	printf("      --use_exp ");
	printf(" Use Experimental verbs in data path. Default is OFF.\n");
	#endif

	#ifdef HAVE_ACCL_VERBS
	printf("      --use_res_domain ");
	printf(" Use shared resource domain\n");

	printf("      --verb_type=<option> ");
	printf(" Set verb type: normal, accl. Default is normal.\n");
	#endif

	if (tst == BW) {
		printf("\n Rate Limiter:\n");
		printf("      --burst_size=<size>");
		printf(" Set the amount of messages to send in a burst when using rate limiter\n");

		printf("      --rate_limit=<rate>");
		printf(" Set the maximum rate of sent packages. default unit is [pps]. use --rate_units to change that.\n");

		printf("      --rate_units=<units>");
		printf(" [Mgp] Set the units for rate limit to MBps (M), Gbps (g) or pps (p). default is pps (p)\n");
	}

	putchar('\n');
}
*/


static void force_dependecies(struct perftest_parameters *user_param){
	/*Additional configuration and assignments.*/
	if (user_param->test_type == ITERATIONS) {

		if (user_param->tx_depth > user_param->iters) {
			user_param->tx_depth = user_param->iters;
		}

		if (user_param->verb == SEND && user_param->rx_depth > user_param->iters) {
			user_param->rx_depth = user_param->iters;
			
		}

		if (user_param->connection_type == UD || user_param->connection_type == UC) {
			if (user_param->rx_depth == DEF_RX_SEND) {
				user_param->rx_depth = (user_param->iters < UC_MAX_RX) ? user_param->iters : UC_MAX_RX;
			}
		}
	}
	

	if (user_param->cq_mod > user_param->tx_depth) {
		user_param->cq_mod = user_param->tx_depth;
	}

	if (user_param->verb == READ || user_param->verb == ATOMIC){
		user_param->inline_size = 0;
	}


	if (user_param->verb == ATOMIC && user_param->size != DEF_SIZE_ATOMIC) {
		user_param->size = DEF_SIZE_ATOMIC;
	}

	if (user_param->use_srq && user_param->verb != SEND) {
		printf(RESULT_LINE);
		printf(" Using SRQ only avavilible in SEND tests.\n");
		exit (1);
	}

	if (user_param->dualport == ON) {

		user_param->num_of_qps *= 2;
		if (user_param->tst != BW) {
			printf(" Dual-port mode only supports BW tests.\n");
			exit (1);
		}

		if (user_param->use_mcg){
			printf(" Dual-port mode not supported in multicast feature\n");
			exit (1);
		}
	}

	if (user_param->post_list > 1) {
		user_param->cq_mod = user_param->post_list;
		printf(RESULT_LINE);
		printf("Post List requested - CQ moderation will be the size of the post list\n");
	}

	// if (user_param->test_type==DURATION) {

		// /* When working with Duration, iters=0 helps us to satisfy loop cond. in run_iter_bw.
		// We also use it for "global" counter of packets.
		// */
		// user_param->iters = 0;
		// user_param->noPeak = ON;

		// if (user_param->use_event) {
			// printf(RESULT_LINE);
			// fprintf(stderr,"Duration mode doesn't work with events.\n");
			// exit(1);
		// }

		// if (user_param->cpu_util) {
			// user_param->cpu_util_data.enable = 1;
		// }
	// }

	// if ( (user_param->test_type != DURATION) && user_param->cpu_util ) {
		// printf(RESULT_LINE);
		// fprintf(stderr, " CPU Utilization works only with Duration mode.\n");
	// }

	if (user_param->use_mcg &&  user_param->gid_index == -1) {
		user_param->gid_index = 0;
	}


	if (user_param->verb == ATOMIC && user_param->connection_type == DC) {
		printf(RESULT_LINE);
		fprintf(stderr, " ATOMIC tests don't support DC transport\n");
		exit(1);
	}

	 if (user_param->tos != DEF_TOS) {
		fprintf(stdout," TOS only valid for rdma_cm based QP and RawEth QP \n");
		exit(1);
	}

	if (user_param->use_mcg) {

		if (user_param->connection_type != UD)
			user_param->connection_type = UD;


		if (user_param->num_of_qps > 1) {
			fprintf(stdout,"Only 1 QP supported in multicast\n");
			exit(1);
		}
	}

	if(user_param->verb == ATOMIC && user_param->use_odp) {
		printf(RESULT_LINE);
		fprintf(stderr," ODP does not support ATOMICS for now\n");
		exit(1);
	}
	

	if (user_param->connection_type == DC && !user_param->use_srq)
		user_param->use_srq = 1;

	

	if ((user_param->use_srq && (user_param->tst == LAT || user_param->machine == SERVER )) || user_param->use_xrc)
		user_param->srq_exists = 1;

	if (user_param->burst_size > 0) {
		if (user_param->is_rate_limiting == 0) {
			printf(RESULT_LINE);
			fprintf(stderr," Can't enable burst mode when rate limiter is off\n");
			exit(1);
		}
	}

	if (user_param->is_rate_limiting == 1) {
		if (user_param->tst != BW || user_param->verb == ATOMIC || (user_param->verb == SEND)) {
			printf(RESULT_LINE);
			fprintf(stderr," Rate limiter cann't be executed on non-BW, ATOMIC or bidirectional SEND tests\n");
			exit(1);
		}

		if (user_param->burst_size <= 0) {
			printf(RESULT_LINE);
			fprintf(stderr," Setting burst size to tx depth = %d\n",user_param->tx_depth);
			user_param->burst_size = user_param->tx_depth;
		}
	}

	if (user_param->output != -1) {
		if (user_param->tst == BW && !(user_param->output == OUTPUT_BW || user_param->output == OUTPUT_MR)) {
			printf(RESULT_LINE);
			fprintf(stderr," Output verbosity level for BW can be: bandwidth, message_rate\n");
			exit(1);
		}

		if (user_param->tst == LAT && !(user_param->output == OUTPUT_LAT)) {
			printf(RESULT_LINE);
			fprintf(stderr," Output verbosity level for LAT can be: latency\n");
			exit(1);
		}
	}

	if ( (user_param->latency_gap > 0) && user_param->tst != LAT ) {
		printf(RESULT_LINE);
		fprintf(stderr," Latency gap feature is only for latency tests\n");
		exit(1);
	}

	if ( user_param->test_type == DURATION && user_param->margin == DEF_INIT_MARGIN) {
		user_param->margin = user_param->duration / 4;
	}

	#if defined(HAVE_VERBS_EXP) && defined(HAVE_DC)
	if (user_param->connection_type == DC) {
		user_param->use_exp = 1;
	}
	#endif

	if ( (user_param->connection_type == UD) && (user_param->inline_size > MAX_INLINE_UD) ) {
		printf(RESULT_LINE);
		fprintf(stderr, "Setting inline size to %d (Max inline size in UD)\n",MAX_INLINE_UD);
		user_param->inline_size = MAX_INLINE_UD;
	}

	if (user_param->report_per_port && (user_param->test_type != DURATION || !user_param->dualport)) {
		printf(RESULT_LINE);
		fprintf(stderr, "report per port feature work only with Duration and Dualport\n");
		exit(1);
	}

	/* WA for a bug when rx_depth is odd in SEND */
	if (user_param->verb == SEND && (user_param->rx_depth % 2 == 1))
		user_param->rx_depth += 1;


	if (!(user_param->duration > 2*user_param->margin)) {
		printf(RESULT_LINE);
		fprintf(stderr, "please check that DURATION > 2*MARGIN\n");
		exit(1);
	}
	
	printf("rx_depth = %d\n", user_param->rx_depth);

	return;
}


//System related
//get
static int get_cache_line_size(){
	int size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
	if (size == 0) {
		#if defined(__sparc__) && defined(__arch64__)
		char* file_name =
			"/sys/devices/system/cpu/cpu0/l2_cache_line_size";
		#else
		char* file_name =
			"/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size";
		#endif

		FILE *fp;
		char line[10];
		fp = fopen(file_name, "r");
		if (fp == NULL) {
			return DEF_CACHE_LINE_SIZE;
		}
		fgets(line,10,fp);
		size = atoi(line);
		fclose(fp);
	}

	if (size <= 0)
		size = DEF_CACHE_LINE_SIZE;

	return size;
}

enum ctx_device ib_dev_name(struct ibv_context *context){
	enum ctx_device dev_fname = UNKNOWN;
	struct ibv_device_attr attr;

	if (ibv_query_device(context,&attr)) {
		dev_fname = DEVICE_ERROR;
	}

	else if (attr.vendor_id == 5157) {

		switch (attr.vendor_part_id >> 12) {
			case 10 :
			case 4  : dev_fname = CHELSIO_T4; break;
			case 11 :
			case 5  : dev_fname = CHELSIO_T5; break;
			default : dev_fname = UNKNOWN; break;
		}

		/* Assuming it's Mellanox HCA or unknown.
		If you want Inline support in other vendor devices, please send patch to gilr@dev.mellanox.co.il
		*/
	} else {

		switch (attr.vendor_part_id) {
			case 4115  : dev_fname = CONNECTX4; break;
			case 4116  : dev_fname = CONNECTX4; break;
			case 4117  : dev_fname = CONNECTX4LX; break;
			case 4118  : dev_fname = CONNECTX4LX; break;
			case 4113  : dev_fname = CONNECTIB; break;
			case 4099  : dev_fname = CONNECTX3; break;
			case 4100  : dev_fname = CONNECTX3; break;
			case 4103  : dev_fname = CONNECTX3_PRO; break;
			case 4104  : dev_fname = CONNECTX3_PRO; break;
			case 26418 : dev_fname = CONNECTX2; break;
			case 26428 : dev_fname = CONNECTX2; break;
			case 26438 : dev_fname = CONNECTX2; break;
			case 26448 : dev_fname = CONNECTX2; break;
			case 26458 : dev_fname = CONNECTX2; break;
			case 26468 : dev_fname = CONNECTX2; break;
			case 26478 : dev_fname = CONNECTX2; break;
			case 25408 : dev_fname = CONNECTX;  break;
			case 25418 : dev_fname = CONNECTX;  break;
			case 25428 : dev_fname = CONNECTX;  break;
			case 25448 : dev_fname = CONNECTX;  break;
			case 1824  : dev_fname = SKYHAWK;  break;
			default	   : dev_fname = UNKNOWN;
		}
	}

	return dev_fname;
}

uint16_t ctx_get_local_lid(struct ibv_context *context,int port){
	struct ibv_port_attr attr;

	if (ibv_query_port(context,port,&attr))
		return 0;

	return attr.lid;
}


//set
static void change_conn_type(int *cptr,VerbType verb,const char *optarg){
	if (strcmp(connStr[0],optarg)==0)
		*cptr = RC;

	else if (strcmp(connStr[1],optarg)==0) {
		*cptr = UC;
		if (verb == READ || verb == ATOMIC) {
			fprintf(stderr," UC connection not possible in READ/ATOMIC verbs\n");
			exit(1);
		}

	} else if (strcmp(connStr[2],optarg)==0)  {
		*cptr = UD;
		if (verb != SEND) {
			fprintf(stderr," UD connection only possible in SEND verb\n");
			exit(1);
		}
	} else if(strcmp(connStr[3],optarg)==0) {

	} else if(strcmp(connStr[4],optarg)==0) {
		#ifdef HAVE_XRCD
		*cptr = XRC;
		#else
		fprintf(stderr," XRC not detected in libibverbs\n");
		exit(1);
		#endif
	} else if (strcmp(connStr[5],optarg)==0) {
		#ifdef HAVE_DC
		*cptr = DC;
		#else
		fprintf(stderr," DC not detected in libibverbs\n");
		exit(1);
		#endif
	} else {
		fprintf(stderr," Invalid Connection type . please choose from {RC,UC,UD}\n");
		exit(1);
	}
}

static uint8_t set_link_layer(struct ibv_context *context,uint8_t ib_port){
	struct ibv_port_attr port_attr;
	uint8_t curr_link;

	if (ibv_query_port(context,ib_port,&port_attr)) {
		fprintf(stderr," Unable to query port attributes\n");
		return LINK_FAILURE;
	}

	if (port_attr.state != IBV_PORT_ACTIVE) {
		fprintf(stderr," Port number %d state is %s\n"
				,ib_port
				,portStates[port_attr.state]);
		return LINK_FAILURE;
	}

	curr_link = port_attr.link_layer;
	if (!strcmp(link_layer_str(curr_link),"Unknown")) {
		fprintf(stderr," Unable to determine link layer \n");
		return LINK_FAILURE;
	}

	return port_attr.link_layer;
}

static void ctx_set_max_inline(struct ibv_context *context,struct perftest_parameters *user_param){
	enum ctx_device current_dev = ib_dev_name(context);

	if (current_dev == UNKNOWN || current_dev == DEVICE_ERROR) {

		if (user_param->inline_size != DEF_INLINE) {
			printf(RESULT_LINE);
			fprintf(stderr,"Device not recognized to implement inline feature. Disabling it\n");
		}
		user_param->inline_size = 0;
		return;
	}

	if (user_param->inline_size == DEF_INLINE) {

		if (user_param->tst ==LAT) {

			switch(user_param->verb) {

				case WRITE: user_param->inline_size = (user_param->connection_type == DC)? DEF_INLINE_DC : DEF_INLINE_WRITE; break;
				case SEND : user_param->inline_size = (user_param->connection_type == DC)? DEF_INLINE_DC : (user_param->connection_type == UD)? DEF_INLINE_SEND_UD : DEF_INLINE_SEND_RC_UC; break;
				default   : user_param->inline_size = 0;
			}

		} else {
			user_param->inline_size = 0;
		}
	}

	return;
}

static int ctx_set_out_reads(struct ibv_context *context,int num_user_reads){
	int max_reads = 0;
	struct ibv_device_attr attr;

	if (!ibv_query_device(context,&attr)) {
		max_reads = attr.max_qp_rd_atom;
	}

	if (num_user_reads > max_reads) {
		printf(RESULT_LINE);
		fprintf(stderr," Number of outstanding reads is above max = %d\n",max_reads);
		fprintf(stderr," Changing to that max value\n");
		num_user_reads = max_reads;
	}
	else if (num_user_reads <= 0) {
		num_user_reads = max_reads;
	}
	printf("In ctx_set_out_reads: num_user_reads =%d\n", max_reads);

	return num_user_reads;
}

enum ibv_mtu set_mtu(struct ibv_context *context,uint8_t ib_port,int user_mtu){
	struct ibv_port_attr port_attr;
	enum ibv_mtu curr_mtu;

	if (ibv_query_port(context,ib_port,&port_attr)) {
		fprintf(stderr," Error when trying to query port\n");
		exit(1);
	}

	/* User did not ask for specific mtu. */
	if (user_mtu == 0) {
		enum ctx_device current_dev = ib_dev_name(context);
		curr_mtu = port_attr.active_mtu;
		/* CX3_PRO and CX3 have a HW bug in 4K MTU, so we're forcing it to be 2K MTU */
		if (curr_mtu == IBV_MTU_4096 && (current_dev == CONNECTX3_PRO || current_dev == CONNECTX3))
			curr_mtu = IBV_MTU_2048;
	}

	else {
		switch (user_mtu) {
			case 256  :	curr_mtu = IBV_MTU_256;	 break;
			case 512  : curr_mtu = IBV_MTU_512;	 break;
			case 1024 :	curr_mtu = IBV_MTU_1024; break;
			case 2048 :	curr_mtu = IBV_MTU_2048; break;
			case 4096 :	curr_mtu = IBV_MTU_4096; break;
			default   :
					fprintf(stderr," Invalid MTU - %d \n",user_mtu);
					fprintf(stderr," Please choose mtu from {256,512,1024,2048,4096}\n");
					fprintf(stderr," Will run with the port active mtu - %d\n",port_attr.active_mtu);
					curr_mtu = port_attr.active_mtu;
		}

		if (curr_mtu > port_attr.active_mtu) {
			fprintf(stdout,"Requested mtu is higher than active mtu \n");
			fprintf(stdout,"Changing to active mtu - %d\n",port_attr.active_mtu);
			curr_mtu = port_attr.active_mtu;
		}
	}
	return curr_mtu;
}



//check
int check_if_valid_udp_port(int udp_port){
	return ON;
}

static int ctx_chk_pkey_index(struct ibv_context *context,int pkey_idx){
	int idx = 0;
	struct ibv_device_attr attr;

	if (!ibv_query_device(context,&attr)) {
		if (pkey_idx > attr.max_pkeys - 1) {
			printf(RESULT_LINE);
			fprintf(stderr," Specified PKey Index, %i, greater than allowed max, %i\n",pkey_idx,attr.max_pkeys - 1);
			fprintf(stderr," Changing to 0\n");
			idx = 0;
		} else
			idx = pkey_idx;
	} else {
		fprintf(stderr," Unable to validata PKey Index, changing to 0\n");
		idx = 0;
	}

	return idx;

}


//parse
int parse_ip_from_str(char *ip, u_int32_t *addr){
	/******************************************************************************
	  parse_ip_from_str.
	 *
	 * Description : Convert from presentation format of an Internet number in nuffer
	 starting at CP to the binary network format and store result for
	 interface type AF in buffer starting at BUF.
	 *
	 *  Parameters :
	 *		*ip - char* ip string.
	 *		*addr - pointer to output array
	 *
	 * Return Value : SUCCESS, FAILURE.
	 *
	 ******************************************************************************/
	return inet_pton(AF_INET, ip, addr);
}

/*
static int parse_mac_from_str(char *mac, u_int8_t *addr){
	char tmpMac[MAC_LEN+1];
	char *tmpField;
	int fieldNum = 0;

	if (strlen(mac) != MAC_LEN) {
		fprintf(stderr, "invalid MAC length\n");
		return FAILURE;
	}
	if (addr == NULL) {
		fprintf(stderr, "invalid  output addr array\n");
		return FAILURE;
	}

	strcpy(tmpMac, mac);
	tmpField = strtok(tmpMac, ":");
	while (tmpField != NULL && fieldNum < MAC_ARR_LEN) {
		char *chk;
		int tmpVal;
		tmpVal = strtoul(tmpField, &chk, HEX_BASE);
		if (tmpVal > 0xff) {
			fprintf(stderr, "field %d value %X out of range\n", fieldNum, tmpVal);
			return FAILURE;
		}
		if (*chk != 0) {
			fprintf(stderr, "Non-digit character %c (%0x) detected in field %d\n", *chk, *chk, fieldNum);
			return FAILURE;
		}
		addr[fieldNum++] = (u_int8_t) tmpVal;
		tmpField = strtok(NULL, ":");
	}
	if (tmpField != NULL || fieldNum != MAC_ARR_LEN) {
		fprintf(stderr, "MAC address longer than six fields\n");
		return FAILURE;
	}
	return SUCCESS;
}
*/
/*
static int parse_ethertype_from_str(char *ether_str, uint16_t *ethertype_val){
	if (strlen(ether_str) != ETHERTYPE_LEN) {
		fprintf(stderr, "invalid ethertype length\n");
		return FAILURE;
	}
	*ethertype_val = strtoul(ether_str, NULL, HEX_BASE);
	if (!*ethertype_val)
		return FAILURE;
	return SUCCESS;
}
*/

const char *link_layer_str(uint8_t link_layer){
	switch (link_layer) {

		case IBV_LINK_LAYER_UNSPECIFIED:
		case IBV_LINK_LAYER_INFINIBAND:
			return "IB";
		case IBV_LINK_LAYER_ETHERNET:
			return "Ethernet";
		#ifdef HAVE_SCIF
		case IBV_LINK_LAYER_SCIF:
			return "SCIF";
		#endif
		default:
			return "Unknown";
	}
}

static inline int ipv6_addr_v4mapped(const struct in6_addr *a){
	return ((a->s6_addr32[0] | a->s6_addr32[1]) |
			(a->s6_addr32[2] ^ htonl(0x0000ffff))) == 0UL ||
			/* IPv4 encoded multicast addresses */
			(a->s6_addr32[0] == htonl(0xff0e0000) &&
			((a->s6_addr32[1] |
			(a->s6_addr32[2] ^ htonl(0x0000ffff))) == 0UL));
}


//IB related

void alloc_ctx(struct pingpong_context *ctx,struct perftest_parameters *user_param){

	int num_of_qps_factor;

	ctx->cycle_buffer = user_param->cycle_buffer;
	ctx->cache_line_size = user_param->cache_line_size;

	ALLOCATE(user_param->port_by_qp, uint64_t, user_param->num_of_qps);


	ALLOCATE(ctx->qp, struct ibv_qp*, user_param->num_of_qps);
	ALLOCATE(ctx->mr, struct ibv_mr*, user_param->num_of_qps);
	ALLOCATE(ctx->buf, uint64_t* , user_param->num_of_qps);


	if (user_param->machine == CLIENT || user_param->tst == LAT) {

		ALLOCATE(ctx->sge_list,struct ibv_sge,user_param->num_of_qps*user_param->post_list);
		ALLOCATE(ctx->exp_wr,struct ibv_exp_send_wr,user_param->num_of_qps*user_param->post_list);
		ALLOCATE(ctx->wr,struct ibv_send_wr,user_param->num_of_qps*user_param->post_list);
		if ((user_param->verb == SEND && user_param->connection_type == UD ) || user_param->connection_type == DC) {
			ALLOCATE(ctx->ah,struct ibv_ah*,user_param->num_of_qps);
		}
	}

	if (user_param->verb == SEND && (user_param->tst == LAT || user_param->machine == SERVER)) {
		printf("Server allocate receive queue\n");
		ALLOCATE(ctx->recv_sge_list,struct ibv_sge,user_param->num_of_qps);
		ALLOCATE(ctx->rwr,struct ibv_recv_wr,user_param->num_of_qps);
		ALLOCATE(ctx->rx_buffer_addr,uint64_t,user_param->num_of_qps);
	}

	ctx->size = user_param->size;

	num_of_qps_factor = (user_param->mr_per_qp) ? 1 : user_param->num_of_qps;
	ctx->buff_size = BUFF_SIZE(ctx->size, ctx->cycle_buffer) * 2 * num_of_qps_factor;

	user_param->buff_size = ctx->buff_size;
	if (user_param->connection_type == UD)
		ctx->buff_size += ctx->cache_line_size;
}


int ctx_init(struct pingpong_context *ctx, struct perftest_parameters *user_param){
	int i;
	int num_of_qps = user_param->num_of_qps / 2;
	
	printf("In ctx_init, num_of_qps = %d\n", num_of_qps);

	struct ibv_exp_device_attr dattr;
	memset(&dattr, 0, sizeof(dattr));
	get_verbs_pointers(ctx);

	ctx->is_contig_supported  = check_for_contig_pages_support(ctx->context);

	/* Allocating an event channel if requested. */
	// if (user_param->use_event) {
		// printf("Allocate event channel\n");
		// ctx->channel = ibv_create_comp_channel(ctx->context);
		// if (!ctx->channel) {
			// fprintf(stderr, "Couldn't create completion channel\n");
			// return FAILURE;
		// }
	// }
	

	/* Allocating the Protection domain. */
	ctx->pd = ibv_alloc_pd(ctx->context);
	if (!ctx->pd) {
		fprintf(stderr, "Couldn't allocate PD\n");
		return FAILURE;
	}


	if (create_mr(ctx, user_param)) {
		fprintf(stderr, "Failed to create MR\n");
	}

	if (create_cqs(ctx, user_param)) {
		fprintf(stderr, "Failed to create CQs\n");
	}


	for (i=0; i < user_param->num_of_qps; i++) {
		if (create_qp_main(ctx, user_param, i, num_of_qps)) {
			fprintf(stderr, "Failed to create QP.\n");
			return FAILURE;
		}
		
		modify_qp_to_init(ctx, user_param, i, num_of_qps);
	}

	return SUCCESS;
}

static void get_verbs_pointers(struct pingpong_context *ctx){
	ctx->exp_post_send_func_pointer = ibv_exp_get_provider_func(ctx->context,IBV_EXP_POST_SEND_FUNC);
	if (!ctx->exp_post_send_func_pointer) {
		fprintf(stderr, "Couldn't get ibv_exp_post_send pointer\n");
		ctx->exp_post_send_func_pointer = &ibv_exp_post_send;
	}
	ctx->post_send_func_pointer = ibv_exp_get_provider_func(ctx->context,IBV_POST_SEND_FUNC);
	if (!ctx->post_send_func_pointer) {
		fprintf(stderr, "Couldn't get ibv_post_send pointer\n");
		ctx->post_send_func_pointer = &ibv_post_send;
	}
	ctx->poll_cq_func_pointer = ibv_exp_get_provider_func(ctx->context,IBV_POLL_CQ_FUNC);
	if (!ctx->poll_cq_func_pointer) {
		fprintf(stderr, "Couldn't get ibv_poll_cq pointer\n");
	}
}

static int check_for_contig_pages_support(struct ibv_context *context){
	int answer;
	struct ibv_exp_device_attr attr;
	memset(&attr,0,sizeof attr);
	if (ibv_exp_query_device(context,&attr)) {
		fprintf(stderr, "Couldn't get device attributes\n");
		return FAILURE;
	}
	answer = ( attr.exp_device_cap_flags &= IBV_EXP_DEVICE_MR_ALLOCATE) ? SUCCESS : FAILURE;
	return answer;
}

int create_mr(struct pingpong_context *ctx, struct perftest_parameters *user_param){
	/* create first MR */
	if (create_single_mr(ctx, user_param, 0)) {
		fprintf(stderr, "failed to create mr\n");
		return 1;
	}

	return 0;
}

int create_single_mr(struct pingpong_context *ctx, struct perftest_parameters *user_param, int qp_index){
	int flags = IBV_ACCESS_LOCAL_WRITE;

	
	// struct ibv_exp_reg_mr_in reg_mr_exp_in;
	// uint64_t exp_flags = IBV_EXP_ACCESS_LOCAL_WRITE;



	/* Allocating buffer for data, in case driver not support contig pages. */
	// if (ctx->is_contig_supported == FAILURE) {
		ctx->buf[qp_index] = memalign(user_param->cycle_buffer, ctx->buff_size);
		if (!ctx->buf[qp_index]) {
			fprintf(stderr, "Couldn't allocate work buf.\n");
			return 1;
		}
		memset(ctx->buf[qp_index], 0, ctx->buff_size);
	// }
	// else{
		// printf("buffer size: %lu\n", ctx->buff_size);
		// ctx->buf[qp_index] = NULL;
		// exp_flags |= IBV_EXP_ACCESS_ALLOCATE_MR;

	// }
	


	flags |= IBV_ACCESS_REMOTE_WRITE;
	flags |= IBV_ACCESS_REMOTE_READ;
	flags |= IBV_ACCESS_REMOTE_ATOMIC;

	// exp_flags |= IBV_EXP_ACCESS_REMOTE_WRITE;
	// exp_flags |= IBV_EXP_ACCESS_REMOTE_READ;
	// exp_flags |= IBV_EXP_ACCESS_REMOTE_ATOMIC;


	/* Allocating Memory region and assigning our buffer to it. */
	// if (ctx->is_contig_supported == SUCCESS){
		// reg_mr_exp_in.pd = ctx->pd;
		// reg_mr_exp_in.addr = ctx->buf[qp_index];
		// reg_mr_exp_in.length = ctx->buff_size;
		// reg_mr_exp_in.exp_access = exp_flags;
		// reg_mr_exp_in.comp_mask = 0;

		// ctx->mr[qp_index] = ibv_exp_reg_mr(&reg_mr_exp_in);
	// }
	// else{
		ctx->mr[qp_index] = ibv_reg_mr(ctx->pd, ctx->buf[qp_index], ctx->buff_size, flags);
	// }
	

	if (!ctx->mr[qp_index]) {
		fprintf(stderr, "Couldn't allocate MR\n");
		return 1;
	}
	
	
	// if (ctx->is_contig_supported == SUCCESS){
		// ctx->buf[qp_index] = ctx->mr[qp_index]->addr;
	// }
	
	memset(ctx->buf[qp_index], 0, ctx->buff_size);
	// uint64_t tmpdata = (uint64_t)*(ctx->buf[qp_index]);
	*ctx->buf[qp_index] = 1;
	printf("local buffer %d address:%p (%p), data %lu\n", qp_index, &(ctx->buf[qp_index][0]), ctx->mr[qp_index]->addr, ctx->buf[qp_index][0]);
	return 0;
}

int create_cqs(struct pingpong_context *ctx, struct perftest_parameters *user_param){
	int ret;
	int dct_only = 0, need_recv_cq = 0;
	int tx_buffer_depth = user_param->tx_depth;

	if (user_param->connection_type == DC) {
		dct_only = (user_param->machine == SERVER && !(user_param->tst == LAT));
		printf("dct_only = %d\n", dct_only);
	}

	if (dct_only){
		tx_buffer_depth = user_param->rx_depth;
		printf("dct_only = %d\n", dct_only);
	}

	if ((user_param->connection_type == DC && !dct_only) || (user_param->verb == SEND)){
		need_recv_cq = 1;
		printf("need_recv_cq = %d\n", need_recv_cq);
	}
	
	ret = create_reg_cqs(ctx, user_param, tx_buffer_depth, need_recv_cq);

	return ret;
}

int create_reg_cqs(struct pingpong_context *ctx, struct perftest_parameters *user_param, int tx_buffer_depth, int need_recv_cq){

	ctx->send_cq = ibv_create_cq(ctx->context,tx_buffer_depth*user_param->num_of_qps, NULL, ctx->channel, 0);
	if (!ctx->send_cq) {
		fprintf(stderr, "Couldn't create CQ\n");
		return FAILURE;
	}

	if (need_recv_cq) {
		printf("Create recv cq\n");
		ctx->recv_cq = ibv_create_cq(ctx->context, user_param->rx_depth*user_param->num_of_qps, NULL, ctx->channel, 0);
		if (!ctx->recv_cq) {
			fprintf(stderr, "Couldn't create a receiver CQ\n");
			return FAILURE;
		}
	}

	return SUCCESS;
}

int create_qp_main(struct pingpong_context *ctx, struct perftest_parameters *user_param, int i, int num_of_qps){
	int ret, query;

	/* flag that indicates that we are going to use exp QP */
	query = (user_param->connection_type == DC);
	query |= user_param->use_xrc;
	// query |= user_param->inline_recv_size != 0;
	query |= user_param->masked_atomics;
	query |= user_param->use_res_domain;
	
	if (query == 1){
		user_param->is_exp_qp = 1;
	}

	ret = create_reg_qp_main(ctx, user_param, i, num_of_qps);

	return ret;
}

int create_reg_qp_main(struct pingpong_context *ctx, struct perftest_parameters *user_param, int i, int num_of_qps){
	printf("In create_reg_qp_main\n");
	ctx->qp[i] = ctx_qp_create(ctx,user_param);
	if (ctx->qp[i] == NULL) {
		fprintf(stderr," Unable to create QP.\n");
		return FAILURE;
	}

	return SUCCESS;
}

struct ibv_qp* ctx_qp_create(struct pingpong_context *ctx, struct perftest_parameters *user_param){
	
	struct ibv_qp_init_attr attr;
	struct ibv_qp* qp = NULL;

	memset(&attr, 0, sizeof(struct ibv_qp_init_attr));
	attr.send_cq = ctx->send_cq;
	attr.recv_cq = (user_param->verb == SEND) ? ctx->recv_cq : ctx->send_cq;
	attr.cap.max_send_wr  = user_param->tx_depth;
	attr.cap.max_send_sge = MAX_SEND_SGE;
	attr.cap.max_inline_data = user_param->inline_size;

	attr.srq = NULL;
	attr.cap.max_recv_wr  = user_param->rx_depth;
	attr.cap.max_recv_sge = MAX_RECV_SGE;

	switch (user_param->connection_type) {

		case RC : 
			attr.qp_type = IBV_QPT_RC; 
			break;
		case UC : 
			attr.qp_type = IBV_QPT_UC; 
			break;
		case UD : 
			attr.qp_type = IBV_QPT_UD; 
			break;
		default:  
			fprintf(stderr, "Unknown connection type \n");
			return NULL;
	}
	

	qp = ibv_create_qp(ctx->pd,&attr);
	
	return qp;
}

int modify_qp_to_init(struct pingpong_context *ctx, struct perftest_parameters *user_param, int qp_index, int num_of_qps) {
	
	uint64_t init_flag = 0;

	if (ctx_modify_qp_to_init(ctx->qp[qp_index],user_param,init_flag)) {
		fprintf(stderr, "Failed to modify QP to INIT\n");
		return FAILURE;
	}

	return SUCCESS;
}

int ctx_modify_qp_to_init(struct ibv_qp *qp,struct perftest_parameters *user_param, uint64_t init_flag){

	int num_of_qps = user_param->num_of_qps;
	int num_of_qps_per_port = user_param->num_of_qps / 2;

	struct ibv_qp_attr attr;
	int flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT;


	struct ibv_exp_qp_attr exp_attr;


	static int portindex=0;  /* for dual-port support */
	int ret = 0;

	memset(&attr, 0, sizeof(struct ibv_qp_attr));
	attr.qp_state        = IBV_QPS_INIT;
	attr.pkey_index      = user_param->pkey_index;


	memset(&exp_attr, 0, sizeof(struct ibv_exp_qp_attr));
	exp_attr.qp_state        = attr.qp_state;
	exp_attr.pkey_index      = attr.pkey_index;



	if (user_param->dualport==ON) {
		if (portindex % num_of_qps < num_of_qps_per_port) {
			attr.port_num = user_param->ib_port;
			user_param->port_by_qp[portindex] = 0;
		} else {
			attr.port_num = user_param->ib_port2;
			user_param->port_by_qp[portindex] = 1;
		}
		portindex++;

	} else {
		attr.port_num = user_param->ib_port;
	}


	exp_attr.port_num = attr.port_num;


	if (user_param->connection_type == UD) {
		attr.qkey = DEFF_QKEY;
		flags |= IBV_QP_QKEY;

	} else {
		switch (user_param->verb) {
			case ATOMIC: 
				attr.qp_access_flags = IBV_ACCESS_REMOTE_ATOMIC; 
				break;
			case READ  : 
				attr.qp_access_flags = IBV_ACCESS_REMOTE_READ;  
				break;
			case WRITE : 
				attr.qp_access_flags = IBV_ACCESS_REMOTE_WRITE; 
				break;
			case SEND  : 
				attr.qp_access_flags = IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE;
		}
		flags |= IBV_QP_ACCESS_FLAGS;
	}



	ret = ibv_modify_qp(qp,&attr,flags);

	if (ret) {
		fprintf(stderr, "Failed to modify QP to INIT, ret=%d\n",ret);
		return 1;
	}
	
	printf("Modify QPs to INIT\n");
	return 0;
}


int ctx_connect(struct pingpong_context *ctx, struct pingpong_dest *dest, struct perftest_parameters *user_param, struct pingpong_dest *my_dest){

	int i;

	struct ibv_exp_qp_attr attr_ex;	
	struct ibv_qp_attr attr;
	int xrc_offset = 0;

	
	for (i=0; i < user_param->num_of_qps; i++) {

		memset(&attr_ex, 0, sizeof attr_ex);
		memset(&attr, 0, sizeof attr);


		if(ctx_modify_qp_to_rtr(ctx->qp[i],&attr,user_param,&dest[xrc_offset + i],&my_dest[i],i)) {
			fprintf(stderr, "Failed to modify QP %d to RTR\n",ctx->qp[i]->qp_num);
			return FAILURE;
		}
		
		printf("is client: %d\n", user_param->machine == CLIENT);
		if (user_param->tst == LAT || user_param->machine == CLIENT) {
	
			if(ctx_modify_qp_to_rts(ctx->qp[i],&attr,user_param,&dest[xrc_offset + i],&my_dest[i])) {
				fprintf(stderr, "Failed to modify QP to RTS\n");
				return FAILURE;
			}
			
		}

		if ((user_param->connection_type == UD || user_param->connection_type == DC) && (user_param->tst == LAT || user_param->machine == CLIENT)) {
			
			ctx->ah[i] = ibv_create_ah(ctx->pd,&(attr.ah_attr));


			if (!ctx->ah[i]) {
				fprintf(stderr, "Failed to create AH for UD\n");
				return FAILURE;
			}
		}

	}
	return SUCCESS;
}

static int ctx_modify_qp_to_rtr(struct ibv_qp *qp, struct ibv_qp_attr *attr, struct perftest_parameters *user_param, struct pingpong_dest *dest, struct pingpong_dest *my_dest, int qpindex){
	
	printf("ctx_modify_qp_to_rtr\n");
	int num_of_qps = user_param->num_of_qps;
	int num_of_qps_per_port = user_param->num_of_qps / 2;

	int flags = IBV_QP_STATE;
	attr->qp_state = IBV_QPS_RTR;
	attr->ah_attr.src_path_bits = 0;

	

	/* first half of qps are for ib_port and second half are for ib_port2
	 * in xrc with bidirectional, the first half of qps are xrc_send qps and
	 * the second half are xrc_recv qps. the first half of the send/recv qps
	 * are for ib_port1 and the second half are for ib_port2
	 */
	if (user_param->dualport == ON && (qpindex % num_of_qps >= num_of_qps_per_port)){
		attr->ah_attr.port_num = user_param->ib_port2;
	}
	else{
		attr->ah_attr.port_num = user_param->ib_port;
	}

	

	attr->ah_attr.dlid = (user_param->dlid) ? user_param->dlid : dest->lid;
	attr->ah_attr.sl = user_param->sl;
		
	if (((attr->ah_attr.port_num == user_param->ib_port) && (user_param->gid_index == DEF_GID_INDEX))
			|| ((attr->ah_attr.port_num == user_param->ib_port2) && (user_param->gid_index2 == DEF_GID_INDEX) && user_param->dualport)) {		
		
		attr->ah_attr.is_global = 0;
	} 
	else {

		attr->ah_attr.is_global  = 1;
		attr->ah_attr.grh.dgid = dest->gid;
		attr->ah_attr.grh.sgid_index = (attr->ah_attr.port_num == user_param->ib_port) ? user_param->gid_index : user_param->gid_index2;
		attr->ah_attr.grh.hop_limit = 1;
	}

	if (user_param->connection_type != UD) {

		attr->path_mtu = user_param->curr_mtu;
		attr->dest_qp_num = dest->qpn;
		attr->rq_psn = dest->psn;
		
		flags |= (IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN);

		if (user_param->connection_type == RC) {
			attr->max_dest_rd_atomic = my_dest->out_reads;
			attr->min_rnr_timer = 12;
			flags |= (IBV_QP_MIN_RNR_TIMER | IBV_QP_MAX_DEST_RD_ATOMIC);
		}
	}
	
	
	return ibv_modify_qp(qp,attr,flags);
}


static int ctx_modify_qp_to_rts(struct ibv_qp *qp, struct ibv_qp_attr *attr, struct perftest_parameters *user_param, struct pingpong_dest *dest, struct pingpong_dest *my_dest){
	printf("ctx_modify_qp_to_rts\n");

	int flags = IBV_QP_STATE;
	attr->qp_state = IBV_QPS_RTS;

	

	flags |= IBV_QP_SQ_PSN;
	attr->sq_psn = my_dest->psn;
	
	if (user_param->connection_type == RC) {

		attr->timeout   = user_param->qp_timeout;
		attr->retry_cnt = 7;
		attr->rnr_retry = 7;
		attr->max_rd_atomic  = dest->out_reads;

		flags |= (IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | IBV_QP_MAX_QP_RD_ATOMIC);
	}
	

	return ibv_modify_qp(qp,attr,flags);
}


void ctx_set_send_wqes(struct pingpong_context *ctx, struct perftest_parameters *user_param, struct pingpong_dest *rem_dest){
	printf("In ctx_set_send_wqes\n");
	ctx_set_send_reg_wqes(ctx,user_param,rem_dest);
}

void ctx_set_send_reg_wqes(struct pingpong_context *ctx, struct perftest_parameters *user_param, struct pingpong_dest *rem_dest){
	printf("In ctx_set_send_reg_wqes\n");
	
	int i,j;
	int num_of_qps = user_param->num_of_qps;
	int xrc_offset = 0;


	for (i = 0; i < num_of_qps ; i++) {
		printf("qps: %d, post_list: %d\n", i, user_param->post_list);
		memset(&ctx->wr[i*user_param->post_list],0,sizeof(struct ibv_send_wr));
		ctx->sge_list[i*user_param->post_list].addr = (uintptr_t)ctx->buf[i];
		printf("local address %p, value: %lu\n", ctx->buf[i], *ctx->buf[i]);


		if (user_param->verb == WRITE || user_param->verb == READ){
			printf("WRITE, READ\n");
			ctx->wr[i*user_param->post_list].wr.rdma.remote_addr   = rem_dest[xrc_offset + i].vaddr;
		}

		else if (user_param->verb == ATOMIC){
			
			ctx->wr[i*user_param->post_list].wr.atomic.remote_addr = rem_dest[xrc_offset + i].vaddr;
			printf("ATOMIC, remote address: %lx\n", ctx->wr[i*user_param->post_list].wr.atomic.remote_addr );
		}


		for (j = 0; j < user_param->post_list; j++) {

			ctx->sge_list[i*user_param->post_list + j].length = user_param->size;

			ctx->sge_list[i*user_param->post_list + j].lkey = ctx->mr[i]->lkey;

			// if (j > 0) {
				// printf("More than one list\n");
				// ctx->sge_list[i*user_param->post_list +j].addr = ctx->sge_list[i*user_param->post_list + (j-1)].addr;
			// }

			ctx->wr[i*user_param->post_list + j].sg_list = &ctx->sge_list[i*user_param->post_list + j];
			ctx->wr[i*user_param->post_list + j].num_sge = MAX_SEND_SGE;
			ctx->wr[i*user_param->post_list + j].wr_id   = i;

			if (j == (user_param->post_list - 1)) {
				ctx->wr[i*user_param->post_list + j].send_flags = IBV_SEND_SIGNALED;
				ctx->wr[i*user_param->post_list + j].next = NULL;
			}

			else {
				ctx->wr[i*user_param->post_list + j].next = &ctx->wr[i*user_param->post_list+j+1];
				ctx->wr[i*user_param->post_list + j].send_flags = 0;
			}

			if (user_param->verb == ATOMIC) {
				ctx->wr[i*user_param->post_list + j].opcode = opcode_atomic_array[user_param->atomicType];
				printf("is FAA: %d\n", ctx->wr[i*user_param->post_list + j].opcode==IBV_WR_ATOMIC_FETCH_AND_ADD);
			}
			else {
				ctx->wr[i*user_param->post_list + j].opcode = opcode_verbs_array[user_param->verb];
			}
			
			if (user_param->verb == WRITE || user_param->verb == READ) {

				ctx->wr[i*user_param->post_list + j].wr.rdma.rkey = rem_dest[xrc_offset + i].rkey;

				// if (j > 0) {

					// ctx->wr[i*user_param->post_list + j].wr.rdma.remote_addr =
						// ctx->wr[i*user_param->post_list + (j-1)].wr.rdma.remote_addr;

				// }

			} 
			else if (user_param->verb == ATOMIC) {

				ctx->wr[i*user_param->post_list + j].wr.atomic.rkey = rem_dest[xrc_offset + i].rkey;
				printf("remote key: %x\n", ctx->wr[i*user_param->post_list + j].wr.atomic.rkey );

				// if (j > 0) {

					// ctx->wr[i*user_param->post_list + j].wr.atomic.remote_addr =
						// ctx->wr[i*user_param->post_list + j-1].wr.atomic.remote_addr;
				// }

				if (user_param->atomicType == FETCH_AND_ADD){
					ctx->wr[i*user_param->post_list + j].wr.atomic.compare_add = ATOMIC_ADD_VALUE;
					printf("ATOMIC_ADD_VALUE: %d\n", ATOMIC_ADD_VALUE);
				}
				else{
					ctx->wr[i*user_param->post_list + j].wr.atomic.compare_add = ATOMIC_ADD_VALUE;
					ctx->wr[i*user_param->post_list + j].wr.atomic.swap = ATOMIC_SWAP_VALUE;
					printf("ATOMIC_SWAP_VALUE: %d\n", ATOMIC_SWAP_VALUE);
				}


			} 
			else if (user_param->verb == SEND) {

				if (user_param->connection_type == UD) {

					ctx->wr[i*user_param->post_list + j].wr.ud.ah = ctx->ah[i];

					ctx->wr[i*user_param->post_list + j].wr.ud.remote_qkey = DEF_QKEY;
					ctx->wr[i*user_param->post_list + j].wr.ud.remote_qpn  = rem_dest[xrc_offset + i].qpn;
					
				}
			}

			if ((user_param->verb == SEND || user_param->verb == WRITE) && user_param->size <= user_param->inline_size)
				ctx->wr[i*user_param->post_list + j].send_flags |= IBV_SEND_INLINE;

		}
	}
}


int ctx_set_recv_wqes(struct pingpong_context *ctx,struct perftest_parameters *user_param){
	
	int			i = 0,j,k;
	int			num_of_qps = user_param->num_of_qps;
	struct ibv_recv_wr	*bad_wr_recv;
	int			size_per_qp = user_param->rx_depth;

	if((user_param->use_xrc || user_param->connection_type == DC) && (user_param->tst == LAT)) {

		i = user_param->num_of_qps / 2;
		num_of_qps /= 2;
	}

	if (user_param->use_srq)
		size_per_qp /= user_param->num_of_qps;

	if (user_param->use_rss) {
		i = 1;
		num_of_qps = 1;
	}
	for (k = 0; i < user_param->num_of_qps; i++,k++) {
		
		ctx->recv_sge_list[i].addr  = (uintptr_t)ctx->buf[i];

		if (user_param->connection_type == UD){
			ctx->recv_sge_list[i].addr += (ctx->cache_line_size - UD_ADDITION);
		}

		ctx->recv_sge_list[i].length = SIZE(user_param->connection_type,user_param->size,1);
		ctx->recv_sge_list[i].lkey   = ctx->mr[i]->lkey;

		ctx->rwr[i].sg_list = &ctx->recv_sge_list[i];
		ctx->rwr[i].wr_id   = i;
		ctx->rwr[i].next    = NULL;
		ctx->rwr[i].num_sge	= MAX_RECV_SGE;


		for (j = 0; j < size_per_qp ; ++j) {


			if (ibv_post_recv(ctx->qp[i],&ctx->rwr[i],&bad_wr_recv)) {
				fprintf(stderr, "Couldn't post recv Qp = %d: counter=%d\n",i,j);
				return 1;
			}
			
			

		}
		printf("In ctx_set_recv_wqes, post a receive\n");
	}
	return 0;
}



int run_iter_lat_atomic(struct pingpong_context *ctx,struct perftest_parameters *user_param){
	
	printf("In run_iter_lat\n");
	uint64_t	scnt = 0;
	int 		ne;
	int		err = 0;

	// struct 		ibv_exp_send_wr *bad_exp_wr = NULL;
	struct 		ibv_send_wr *bad_wr = NULL;
	struct 		ibv_wc wc;

	// int 		cpu_mhz = get_cpu_mhz(user_param->cpu_freq_f);
	// int 		total_gap_cycles = user_param->latency_gap * cpu_mhz;
	// cycles_t 	end_cycle, start_gap=0;



	ctx->wr[0].sg_list->length = user_param->size;
	ctx->wr[0].send_flags = IBV_SEND_SIGNALED;


	/* Duration support in latency tests. */
	// if (user_param->test_type == DURATION) {
		// duration_param=user_param;
		// duration_param->state = START_STATE;
		// signal(SIGALRM, catch_alarm);
		// user_param->iters = 0;
		// if (user_param->margin > 0)
			// alarm(user_param->margin);
		// else
			// catch_alarm(0);
	// }

	// while (scnt < user_param->iters || (user_param->test_type == DURATION && user_param->state != END_STATE)) {
		// if (user_param->latency_gap) {
			// start_gap = get_cycles();
			// end_cycle = start_gap + total_gap_cycles;
			// while (get_cycles() < end_cycle) {
				// continue;
			// }
		// }
		// if (user_param->test_type == ITERATIONS)
			// user_param->tposted[scnt++] = get_cycles();

	
		printf("post_send_func_pointer\n");
		err = (ctx->post_send_func_pointer)(ctx->qp[0],&ctx->wr[0],&bad_wr);		
		if (err) {
			fprintf(stderr,"Couldn't post send: scnt=%lu\n",scnt);
			return 1;
		}

		// if (user_param->test_type == DURATION && user_param->state == END_STATE){
			// printf("First duration check\n");
			// break;
		// }
		
		do {
			ne = ibv_poll_cq(ctx->send_cq, 1, &wc);

			if(ne > 0) {
				if (wc.status != IBV_WC_SUCCESS) {
					NOTIFY_COMP_ERROR_SEND(wc,scnt,scnt);
					return 1;
				}
				// printf("here\n");
				// if (user_param->test_type==DURATION && user_param->state == SAMPLE_STATE){
					// printf("Second duration check\n");
					// user_param->iters++;
				// }

			} else if (ne < 0) {
				fprintf(stderr, "poll CQ failed %d\n", ne);
				return FAILURE;
			}

		} while (!user_param->use_event && ne == 0);
		
		printf("%lu\n", *ctx->buf[0]);
	// }

	return 0;
}

int run_iter_lat_write(struct pingpong_context *ctx,struct perftest_parameters *user_param){
	uint64_t                scnt = 0;
	uint64_t                ccnt = 0;
	int                     ne;
	int			err = 0;
	struct ibv_send_wr      *bad_wr = NULL;
	struct ibv_wc           wc;


	ctx->wr[0].sg_list->length = user_param->size;
	ctx->wr[0].send_flags      = IBV_SEND_SIGNALED;
	if (user_param->size <= user_param->inline_size){
		ctx->wr[0].send_flags |= IBV_SEND_INLINE;
	}


	// Done with setup. Start the test. 
	
	err = (ctx->post_send_func_pointer)(ctx->qp[0],&ctx->wr[0],&bad_wr);
	if (err) {
		fprintf(stderr,"Couldn't post send: scnt=%lu\n",scnt);
		return 1;
	}

	do{ 
		ne = ibv_poll_cq(ctx->send_cq, 1, &wc); 
		if(ne > 0) {
			if (wc.status != IBV_WC_SUCCESS) {
				NOTIFY_COMP_ERROR_SEND(wc,scnt,ccnt);
				return 1;
			}
		} 
		else if (ne < 0) {
			fprintf(stderr, "poll CQ failed %d\n", ne);
			return FAILURE;
		}
	} while (ne == 0);

	
	return 0;
}



int run_iter_lat_send(struct pingpong_context *ctx,struct perftest_parameters *user_param){
	printf("In run_iter_lat_send\n");
	uint64_t		scnt = 0; //sent packets counter
	uint64_t		rcnt = 0; // received packets counter
	int			ne;
	int			err = 0;
	struct ibv_wc		wc;
	struct ibv_recv_wr	*bad_wr_recv;
	struct ibv_send_wr	*bad_wr;
	struct ibv_wc s_wc;
	int s_ne;
	
	ctx->wr[0].sg_list->length = user_param->size;
	ctx->wr[0].send_flags = 0;
	

	if (user_param->size <= user_param->inline_size) {
		ctx->wr[0].send_flags |= IBV_SEND_INLINE;
	}

	if (user_param->machine == SERVER) {
		printf("I am Server\n");
		do {
			ne = ibv_poll_cq(ctx->recv_cq,1,&wc);

			if (ne > 0) {
				if (wc.status != IBV_WC_SUCCESS) {
					NOTIFY_COMP_ERROR_RECV(wc,rcnt);
					return 1;
				}
				
				if (ibv_post_recv(ctx->qp[wc.wr_id],&ctx->rwr[wc.wr_id],&bad_wr_recv)) {
					fprintf(stderr, "Couldn't post recv: rcnt=%lu\n",rcnt);
					return 15;
				}
			} 
			else if (ne < 0) {
				fprintf(stderr, "poll CQ failed %d\n", ne);
				return 1;
			}
		} while (!user_param->use_event && ne == 0);
		printf("Success receive: %lu\n", *ctx->buf[0]);
	
	}
	else{
		ctx->wr[0].send_flags |= IBV_SEND_SIGNALED;
		
		err = (ctx->post_send_func_pointer)(ctx->qp[0],&ctx->wr[0],&bad_wr);		
		if (err) {
			fprintf(stderr,"Couldn't post send: scnt=%lu \n",scnt);
			return 1;
		}
		
		usleep(1);
		
		do {
			s_ne = ibv_poll_cq(ctx->send_cq, 1, &s_wc);
			if (s_ne < 0) {
				fprintf(stderr, "poll on Send CQ failed %d\n", s_ne);
				return FAILURE;
			}

			if (s_wc.status != IBV_WC_SUCCESS) {
				printf("Post not success\n");
				NOTIFY_COMP_ERROR_SEND(s_wc,scnt,scnt)
				return 1;
			}
		} while (!user_param->use_event && s_ne == 0);


		
		
		
		
	}
	

	return 0;
}



int ctx_close_connection(struct perftest_comm *comm, struct pingpong_dest *my_dest, struct pingpong_dest *rem_dest){
	printf("In ctx_close_connection\n");
	/*Signal client is finished.*/
	if (ctx_hand_shake(comm,my_dest,rem_dest)) {
		return 1;
	}

	
	if (write(comm->rdma_params->sockfd,"done",sizeof "done") != sizeof "done") {
		perror(" Client write");
		fprintf(stderr,"Couldn't write to socket\n");
		return -1;
	}

	close(comm->rdma_params->sockfd);

	return 0;
}

//ethernet related
int check_mtu(struct ibv_context *context,struct perftest_parameters *user_param, struct perftest_comm *user_comm){
	int curr_mtu=0;


	
	curr_mtu = (int) (set_mtu(context,user_param->ib_port,user_param->mtu));
		
	user_param->curr_mtu = (enum ibv_mtu)(curr_mtu);
		
	

	if (user_param->connection_type == UD && user_param->size > MTU_SIZE(user_param->curr_mtu)) {
		user_param->size = MTU_SIZE(user_param->curr_mtu);
	}


	return SUCCESS;
}

int establish_connection(struct perftest_comm *comm){
	int (*ptr)(struct perftest_comm*);

	ptr = comm->rdma_params->servername ? &ethernet_client_connect : &ethernet_server_connect;

	if ((*ptr)(comm)) {
		fprintf(stderr,"Unable to open file descriptor for socket connection");
		return 1;
	}

	return 0;
}

static int ethernet_client_connect(struct perftest_comm *comm){
	struct addrinfo *res, *t;
	struct addrinfo hints;
	char *service;

	int sockfd = -1;
	memset(&hints, 0, sizeof hints);
	hints.ai_family   = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if (check_add_port(&service,comm->rdma_params->port,comm->rdma_params->servername,&hints,&res)) {
		fprintf(stderr, "Problem in resolving basic address and port\n");
		return 1;
	}

	for (t = res; t; t = t->ai_next) {
		sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);

		if (sockfd >= 0) {
			if (!connect(sockfd, t->ai_addr, t->ai_addrlen))
				break;
			close(sockfd);
			sockfd = -1;
		}
	}

	freeaddrinfo(res);

	if (sockfd < 0) {
		fprintf(stderr, "Couldn't connect to %s:%d\n",comm->rdma_params->servername,comm->rdma_params->port);
		return 1;
	}

	comm->rdma_params->sockfd = sockfd;
	return 0;
}

static int ethernet_server_connect(struct perftest_comm *comm){
	struct addrinfo *res, *t;
	struct addrinfo hints;
	char *service;
	int n;

	int sockfd = -1, connfd;
	memset(&hints, 0, sizeof hints);
	hints.ai_flags    = AI_PASSIVE;
	hints.ai_family   = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if (check_add_port(&service,comm->rdma_params->port,NULL,&hints,&res)) {
		fprintf(stderr, "Problem in resolving basic adress and port\n");
		return 1;
	}

	for (t = res; t; t = t->ai_next) {
		sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);

		if (sockfd >= 0) {
			n = 1;
			setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof n);
			if (!bind(sockfd, t->ai_addr, t->ai_addrlen))
				break;
			close(sockfd);
			sockfd = -1;
		}
	}
	freeaddrinfo(res);

	if (sockfd < 0) {
		fprintf(stderr, "Couldn't listen to port %d\n", comm->rdma_params->port);
		return 1;
	}

	listen(sockfd, 1);
	connfd = accept(sockfd, NULL, 0);

	if (connfd < 0) {
		perror("server accept");
		fprintf(stderr, "accept() failed\n");
		close(sockfd);
		return 1;
	}
	close(sockfd);
	comm->rdma_params->sockfd = connfd;
	return 0;
}

int check_add_port(char **service, int port, const char *servername, struct addrinfo *hints, struct addrinfo **res){
	int number;

	if (asprintf(service,"%d", port) < 0) {
		return FAILURE;
	}

	number = getaddrinfo(servername,*service,hints,res);

	if (number < 0) {
		fprintf(stderr, "%s for %s:%d\n", gai_strerror(number), servername, port);
		return FAILURE;
	}

	return SUCCESS;
}

int set_up_connection(struct pingpong_context *ctx, struct perftest_parameters *user_param, struct pingpong_dest *my_dest) {
	int num_of_qps = user_param->num_of_qps;
	int num_of_qps_per_port = user_param->num_of_qps / 2;

	int i;
	int is_ipv4;

	union ibv_gid temp_gid;
	union ibv_gid temp_gid2;
	struct ibv_port_attr attr;

	srand48(getpid() * time(NULL));


	if (user_param->gid_index != -1) {
		if (ibv_query_port(ctx->context,user_param->ib_port,&attr)){
			
			return 0;
		}
		printf("ibv_query_port\n");

		if (user_param->use_gid_user) {
			if (ibv_query_gid(ctx->context,user_param->ib_port,user_param->gid_index,&temp_gid)) {
				return -1;
			}
			printf("ibv_query_gid\n");
		} else {
			for (i=0 ; i < attr.gid_tbl_len; i++) {
				if (ibv_query_gid(ctx->context,user_param->ib_port,i,&temp_gid)) {	
					return -1;
				}
				is_ipv4 = ipv6_addr_v4mapped((struct in6_addr *)temp_gid.raw);
				printf("is_ipv4\n");
				if ((user_param->ipv6 && !is_ipv4) || (!user_param->ipv6 && is_ipv4)) {
					user_param->gid_index = i;
					break;
				}
			}
		}
	}
	
	if (user_param->dualport==ON) {
		printf("dualport==ON\n");
		if (user_param->gid_index2 != -1) {
			if (ibv_query_port(ctx->context,user_param->ib_port2,&attr))
				return 0;

			if (user_param->use_gid_user) {
				if (ibv_query_gid(ctx->context,user_param->ib_port2,user_param->gid_index,&temp_gid2))
					return -1;
			} else {
				for (i=0 ; i < attr.gid_tbl_len; i++) {
					if (ibv_query_gid(ctx->context,user_param->ib_port2,i,&temp_gid2)) {
						return -1;
					}
					is_ipv4 = ipv6_addr_v4mapped((struct in6_addr *)temp_gid2.raw);
					if ((user_param->ipv6 && !is_ipv4) || (!user_param->ipv6 && is_ipv4)) {
						user_param->gid_index = i;
						break;
					}
				}
			}
		}
	}

	for (i = 0; i < user_param->num_of_qps; i++) {

		if (user_param->dualport == ON) {
			/*first half of qps are for ib_port and second half are for ib_port2
			in xrc with bidirectional, the first half of qps are xrc_send qps and
			the second half are xrc_recv qps. the first half of the send/recv qps
			are for ib_port1 and the second half are for ib_port2
			*/
			if (i % num_of_qps < num_of_qps_per_port) {
				my_dest[i].lid   = ctx_get_local_lid(ctx->context,user_param->ib_port);
				my_dest[i].gid_index = user_param->gid_index;
			} else {
				my_dest[i].lid   = ctx_get_local_lid(ctx->context,user_param->ib_port2);
				my_dest[i].gid_index = user_param->gid_index2;
			}
			/*single-port case*/
		} else {
			my_dest[i].lid   = ctx_get_local_lid(ctx->context,user_param->ib_port);
			my_dest[i].gid_index = user_param->gid_index;
		}

		my_dest[i].qpn   = ctx->qp[i]->qp_num;
		my_dest[i].psn   = lrand48() & 0xffffff;
		my_dest[i].rkey  = ctx->mr[i]->rkey;

		/* Each qp gives his receive buffer address.*/
		my_dest[i].out_reads = user_param->out_reads;
		// if (user_param->mr_per_qp)
			// my_dest[i].vaddr = (uintptr_t)ctx->buf[i] + BUFF_SIZE(ctx->size,ctx->cycle_buffer);
		// else
			// my_dest[i].vaddr = (uintptr_t)ctx->buf[0] + (user_param->num_of_qps + i)*BUFF_SIZE(ctx->size,ctx->cycle_buffer);
		my_dest[i].vaddr = (uintptr_t)ctx->buf[i];
		if (user_param->dualport==ON) {

			if (i % num_of_qps < num_of_qps_per_port)
				memcpy(my_dest[i].gid.raw,temp_gid.raw ,16);

			else
				memcpy(my_dest[i].gid.raw,temp_gid2.raw ,16);
		} else {
			memcpy(my_dest[i].gid.raw,temp_gid.raw ,16);
		}
		
		

	}

	
	return 0;
}

void ctx_print_pingpong_data(struct pingpong_dest *element, struct perftest_comm *comm){
	
	int is_there_mgid,local_mgid,remote_mgid;

	/* use dlid value from user (if user specified and only on the remote side) */
	uint16_t dlid = (comm->rdma_params->dlid && comm->rdma_params->side) ? comm->rdma_params->dlid : element->lid;


	/*First of all we print the basic format.*/
	printf(BASIC_ADDR_FMT, sideArray[comm->rdma_params->side], dlid, element->qpn, element->psn);

	switch (comm->rdma_params->verb) {
		case 2  : printf(READ_FMT,element->out_reads);
		case 1  : printf(RDMA_FMT,element->rkey,element->vaddr);
		default : ;
	}
	
	printf(RDMA_FMT,element->rkey,element->vaddr);

	

	putchar('\n');

	local_mgid    = (comm->rdma_params->side == 0)  && (comm->rdma_params->machine == 0);
	remote_mgid   = (comm->rdma_params->side == 1)  && (comm->rdma_params->machine == 1);
	is_there_mgid = remote_mgid || local_mgid;

	if ((comm->rdma_params->gid_index > -1 || (comm->rdma_params->use_mcg && is_there_mgid))) {

		printf(PERF_GID_FMT,gidArray[comm->rdma_params->use_mcg && is_there_mgid],
				element->gid.raw[0], element->gid.raw[1],
				element->gid.raw[2], element->gid.raw[3],
				element->gid.raw[4], element->gid.raw[5],
				element->gid.raw[6], element->gid.raw[7],
				element->gid.raw[8], element->gid.raw[9],
				element->gid.raw[10],element->gid.raw[11],
				element->gid.raw[12],element->gid.raw[13],
				element->gid.raw[14],element->gid.raw[15]);
	}
}

int ctx_hand_shake(struct perftest_comm *comm, struct pingpong_dest *my_dest, struct pingpong_dest *rem_dest){
	
	int (*read_func_ptr) (struct pingpong_dest*,struct perftest_comm*);
	int (*write_func_ptr)(struct pingpong_dest*,struct perftest_comm*);

	
	read_func_ptr  = &ethernet_read_keys;
	write_func_ptr = &ethernet_write_keys;

	rem_dest->gid_index = my_dest->gid_index;
	if (comm->rdma_params->servername) {
		if ((*write_func_ptr)(my_dest,comm)) {
			fprintf(stderr," Unable to write to socket/rdam_cm\n");
			return 1;
		}
		if ((*read_func_ptr)(rem_dest,comm)) {
			fprintf(stderr," Unable to read from socket/rdam_cm\n");
			return 1;
		}

		/*Server side will wait for the client side to reach the write function.*/
	} else {

		if ((*read_func_ptr)(rem_dest,comm)) {
			fprintf(stderr," Unable to read to socket/rdam_cm\n");
			return 1;
		}
		if ((*write_func_ptr)(my_dest,comm)) {
			fprintf(stderr," Unable to write from socket/rdam_cm\n");
			return 1;
		}
	}

	return 0;
}

static int ethernet_read_keys(struct pingpong_dest *rem_dest, struct perftest_comm *comm){
	// printf("In ethernet_read_keys\n");
	
	if (rem_dest->gid_index == -1) {

		int parsed;
		char msg[KEY_MSG_SIZE];

		if (read(comm->rdma_params->sockfd, msg, sizeof msg) != sizeof msg) {
			fprintf(stderr, "ethernet_read_keys: Couldn't read remote address\n");
			return 1;
		}
		
		// printf("%s\n", msg);

		parsed = sscanf(msg,KEY_PRINT_FMT,(unsigned int*)&rem_dest->lid,
				&rem_dest->out_reads,&rem_dest->qpn,
				&rem_dest->psn, &rem_dest->rkey,&rem_dest->vaddr,&rem_dest->srqn);

		if (parsed != 7) {
			fprintf(stderr, "Couldn't parse line <%.*s>\n",(int)sizeof msg, msg);
			return 1;
		}

	} else {
		//not enter
		char msg[KEY_MSG_SIZE_GID];
		char *pstr = msg, *term;
		char tmp[120];
		int i;

		if (read(comm->rdma_params->sockfd, msg, sizeof msg) != sizeof msg) {
			fprintf(stderr, "ethernet_read_keys: Couldn't read remote address\n");
			return 1;
		}
		
		printf("%s\n", msg);

		term = strpbrk(pstr, ":");
		memcpy(tmp, pstr, term - pstr);
		tmp[term - pstr] = 0;
		rem_dest->lid = (int)strtol(tmp, NULL, 16); /*LID*/

		pstr += term - pstr + 1;
		term = strpbrk(pstr, ":");
		memcpy(tmp, pstr, term - pstr);
		tmp[term - pstr] = 0;
		rem_dest->out_reads = (int)strtol(tmp, NULL, 16); /*OUT_READS*/

		pstr += term - pstr + 1;
		term = strpbrk(pstr, ":");
		memcpy(tmp, pstr, term - pstr);
		tmp[term - pstr] = 0;
		rem_dest->qpn = (int)strtol(tmp, NULL, 16); /*QPN*/

		pstr += term - pstr + 1;
		term = strpbrk(pstr, ":");
		memcpy(tmp, pstr, term - pstr);
		tmp[term - pstr] = 0;
		rem_dest->psn = (int)strtol(tmp, NULL, 16); /*PSN*/

		pstr += term - pstr + 1;
		term = strpbrk(pstr, ":");
		memcpy(tmp, pstr, term - pstr);
		tmp[term - pstr] = 0;
		rem_dest->rkey = (unsigned)strtoul(tmp, NULL, 16); /*RKEY*/

		pstr += term - pstr + 1;
		term = strpbrk(pstr, ":");
		memcpy(tmp, pstr, term - pstr);
		tmp[term - pstr] = 0;

		rem_dest->vaddr = strtoull(tmp, NULL, 16); /*VA*/

		for (i = 0; i < 15; ++i) {
			pstr += term - pstr + 1;
			term = strpbrk(pstr, ":");
			memcpy(tmp, pstr, term - pstr);
			tmp[term - pstr] = 0;

			rem_dest->gid.raw[i] = (unsigned char)strtoll(tmp, NULL, 16);
		}

		pstr += term - pstr + 1;

		strcpy(tmp, pstr);
		rem_dest->gid.raw[15] = (unsigned char)strtoll(tmp, NULL, 16);


		pstr += term - pstr + 4;

		term = strpbrk(pstr, ":");
		memcpy(tmp, pstr, term - pstr);
		tmp[term - pstr] = 0;
		rem_dest->srqn = (unsigned)strtoul(tmp, NULL, 16); /*SRQN*/

	}
	return 0;
}


static int ethernet_write_keys(struct pingpong_dest *my_dest, struct perftest_comm *comm){
	if (my_dest->gid_index == -1) {

		char msg[KEY_MSG_SIZE];

		sprintf(msg,KEY_PRINT_FMT,my_dest->lid,my_dest->out_reads,
				my_dest->qpn,my_dest->psn, my_dest->rkey, my_dest->vaddr, my_dest->srqn);

		if (write(comm->rdma_params->sockfd,msg,sizeof msg) != sizeof msg) {
			perror("client write");
			fprintf(stderr, "Couldn't send local address\n");
			return 1;
		}

	} else {
		//not enter
		char msg[KEY_MSG_SIZE_GID];
		sprintf(msg,KEY_PRINT_FMT_GID, my_dest->lid,my_dest->out_reads,
				my_dest->qpn,my_dest->psn, my_dest->rkey, my_dest->vaddr,
				my_dest->gid.raw[0],my_dest->gid.raw[1],
				my_dest->gid.raw[2],my_dest->gid.raw[3],
				my_dest->gid.raw[4],my_dest->gid.raw[5],
				my_dest->gid.raw[6],my_dest->gid.raw[7],
				my_dest->gid.raw[8],my_dest->gid.raw[9],
				my_dest->gid.raw[10],my_dest->gid.raw[11],
				my_dest->gid.raw[12],my_dest->gid.raw[13],
				my_dest->gid.raw[14],my_dest->gid.raw[15],
				my_dest->srqn);

		if (write(comm->rdma_params->sockfd, msg, sizeof msg) != sizeof msg) {
			perror("client write");
			fprintf(stderr, "Couldn't send local address\n");
			return 1;
		}

	}

	return 0;
}







#include <err.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mosquitto.h>
#include <time.h>

struct userdata {
	char **topics;
	size_t topic_count;
	int command_argc;
	int verbose;
	char **command_argv;
	int qos;
  char **pubtopic;
};

void log_cb(struct mosquitto *mosq, void *obj, int level, const char *str)
{
	printf("%s\n", str);
}

void message_cb(struct mosquitto *mosq, void *obj,
		const struct mosquitto_message *msg)
{
	if (msg->payload) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    struct userdata *ud = (struct userdata *)obj;
    printf("---------------------------\n%s  |  %d-%d-%d %d:%d:%d\n---------------------------\n", msg->payload, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    FILE *cmd=popen(msg->payload, "r");//run command and print output. Can be sent to pub
    char result[1024]={0x0};
    while (fgets(result, sizeof(result), cmd) !=NULL) {
      printf("%s", result);
			mosquitto_publish(mosq, NULL, ud->pubtopic[0], sizeof result, result, 0, false);
		}
    pclose(cmd);
    printf("\n");

	}
}

void connect_cb(struct mosquitto *mosq, void *obj, int result)
{
	struct userdata *ud = (struct userdata *)obj;
	fflush(stderr);
	if (result == 0) {
		size_t i;
		for (i = 0; i < ud->topic_count; i++)
			mosquitto_subscribe(mosq, NULL, ud->topics[i], ud->qos);
	} else {
		fprintf(stderr, "%s\n", mosquitto_connack_string(result));
	}
}

int mqtte_usage(int retcode)
{
	int major, minor, rev;

	mosquitto_lib_version(&major, &minor, &rev);
	printf(
"usage: sudo ./mq [ARGS...] -t TOPIC\n"
"\n"
"options:\n"
" -c,--disable-clean-session  Disable the 'clean session' flag\n"
" -d,--debug                  Enable debugging\n"
" -h,--host HOST              Connect to HOST. Default is localhost\n"
" -k,--keepalive SEC          Set keepalive to SEC. Default is 60\n"
" -p,--port PORT              Set TCP port to PORT. Default is 1883\n"
" -q,--qos QOS                Set Quality of Serive to level. Default is 0\n"
" -t,--topic TOPIC            Set MQTT topic to TOPIC. May be repeated\n"
		"\n", major, minor, rev);
	return retcode;
}

static int perror_ret(const char *msg)
{
	perror(msg);
	return 1;
}

static int valid_qos_range(int qos, const char *type)
{
	if (qos >= 0 && qos <= 2)
		return 1;

	fprintf(stderr, "%d: %s out of range\n", qos, type);
	return 0;
}

int main(int argc, char *argv[])
{
	static struct option opts[] = {
		{"disable-clean-session", no_argument,	0, 'c' },
		{"debug",	no_argument,		0, 'd' },
		{"host",	required_argument,	0, 'h' },
		{"keepalive",	required_argument,	0, 'k' },
		{"port",	required_argument,	0, 'p' },
		{"qos",		required_argument,	0, 'q' },
		{"topic",	required_argument,	0, 't' }
	};
	int debug = 0;
	bool clean_session = true;
	const char *host = "localhost";
	int port = 1883;
	int keepalive = 60;
	int i, c, rc = 1;
	struct userdata ud;
	char hostname[256];
	static char id[MOSQ_MQTT_ID_MAX_LENGTH+1];
	struct mosquitto *mosq = NULL;

	memset(&ud, 0, sizeof(ud));

	memset(hostname, 0, sizeof(hostname));
	memset(id, 0, sizeof(id));

	while ((c = getopt_long(argc, argv, "cdh:k:p:q:t", opts, &i)) != -1) {
		switch(c) {
		case 'c':
			clean_session = false;
			break;
		case 'd':
			debug = 1;
			break;
		case 'h':
			host = optarg;
			break;
		case 'k':
			keepalive = atoi(optarg);
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 'q':
			ud.qos = atoi(optarg);
			if (!valid_qos_range(ud.qos, "QoS"))
				return 1;
			break;
		case 't':
			ud.topic_count++;
			ud.topics = realloc(ud.topics,
					    sizeof(char *) * ud.topic_count);
			ud.topics[ud.topic_count-1] = optarg;
			break;
		}
	}

	if ((ud.topics == NULL)) {
    ud.topic_count++;
    ud.topics = realloc(ud.topics,
          sizeof(char *) * ud.topic_count);
    ud.topics[ud.topic_count-1] = "shell";
    ud.pubtopic = realloc(ud.pubtopic,
          sizeof(char *) * 1);
    ud.pubtopic[0] = "data";
  }

	// id
	gethostname(hostname, sizeof(hostname)-1);
	snprintf(id, sizeof(id), "bbn/%x-%s", getpid(), hostname); //ie: bbn/5241-router

	mosquitto_lib_init();
	mosq = mosquitto_new(id, clean_session, &ud);
	if (mosq == NULL)
		return perror_ret("mosquitto_new");

	if (debug) {
		printf("host=%s:%d\nid=%s\n",
			host, port, id);
		mosquitto_log_callback_set(mosq, log_cb);
	}

	mosquitto_connect_callback_set(mosq, connect_cb);
	mosquitto_message_callback_set(mosq, message_cb);

	//kernel assfucks children here
	signal(SIGCHLD, SIG_IGN);

	rc = mosquitto_connect(mosq, host, port, keepalive);
	if (rc != MOSQ_ERR_SUCCESS) {
		if (rc == MOSQ_ERR_ERRNO)
			return perror_ret("mosquitto_connect_bind");
		fprintf(stderr, "I'm sorry dave, I can't do that. (%d)\n", rc);
		goto cleanup;
	}

	rc = mosquitto_loop_forever(mosq, -1, 1);

cleanup:
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();
	return rc;

}


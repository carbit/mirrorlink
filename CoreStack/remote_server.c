#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "libxml/parser.h"
#include "libxml/tree.h"

#include "../Platform/http_client.h"
#include "../Utils/str.h"

enum {
	ACTION_GET_APPLICATION_LIST,
	ACTION_LAUNCH_APPLICATION,
	ACTION_SET_CLIENT_PROFILE,
	ACTION_MAX
};

enum {
	SERVICE_TYPE_APP,
	SERVICE_TYPE_CLP,
	SERVICE_TYPE_NOT,
	SERVICE_TYPE_MAX
};

struct service_info {
	str_t surl;
	str_t curl;
	str_t eurl;
};

struct remote_server {
	str_t ip;
	str_t uuid;
	uint16_t port;
	struct service_info sinfo[SERVICE_TYPE_MAX];
};

typedef uint8_t (*action_parser)(struct remote_server *server, char *content);

struct action {
	uint32_t stype;
	char *name;
	action_parser handler;
};

static uint16_t remote_server_invoke_action(struct remote_server *server, uint16_t stype, char *action, char *args, action_parser handler);
static uint8_t get_application_list_parse(struct remote_server *server, char *content);
static uint8_t launch_application_parse(struct remote_server *server, char *content);
static uint8_t set_client_profile_parse(struct remote_server *server, char *content);

static struct action action_map[ACTION_MAX] = {
	{SERVICE_TYPE_APP, "GetApplicationList", get_application_list_parse},
	{SERVICE_TYPE_APP, "LaunchApplication", launch_application_parse},
	{SERVICE_TYPE_CLP, "SetClientProfile", set_client_profile_parse}
};


struct remote_server *remote_server_create(const char *ip, uint16_t port, const char *path)
{
	struct http_req *rq = http_client_make_req("GET", path);
	struct http_rsp *rp = http_client_send(ip, port, rq);
	struct remote_server *server = 0;
	if (rp && (200 == http_client_get_errcode(rp))) {
		xmlDocPtr doc = xmlParseDoc(http_client_get_body(rp));
		xmlNodePtr root = xmlDocGetRootElement(doc);
		xmlNodePtr c1 = xmlFirstElementChild(root);
		printf("get description successfully\n");
		for (; c1 != 0; c1 = xmlNextElementSibling(c1)) {
			if (xmlStrEqual(c1->name, xmlCharStrdup("device"))) {
				xmlNodePtr c2 = xmlFirstElementChild(c1);
				for (; c2 != 0; c2 = xmlNextElementSibling(c2)) {
					if (xmlStrEqual(c2->name, xmlCharStrdup("deviceType")) && xmlStrEqual(xmlNodeGetContent(c2), xmlCharStrdup("urn:schemas-upnp-org:device:TmServerDevice:1"))) {
						printf("this is a mirrorlink server device.\n");
						server = (struct remote_server *)calloc(1, sizeof(*server));
						str_append(&(server->ip), ip);
						server->port = port;
					} else if (xmlStrEqual(c2->name, xmlCharStrdup("UDN"))) {
						str_append(&(server->uuid), xmlNodeGetContent(c2));
						printf("uuid = %s\n", server->uuid);
					} else if (xmlStrEqual(c2->name, xmlCharStrdup("serviceList"))) {
						xmlNodePtr c3 = xmlFirstElementChild(c2);
						for (; c3 != 0; c3 = xmlNextElementSibling(c3)) {
							if (xmlStrEqual(c3->name, xmlCharStrdup("service"))) {
								xmlNodePtr c4 = xmlFirstElementChild(c3);
								uint8_t type = SERVICE_TYPE_MAX;
								for (; c4 != 0; c4 = xmlNextElementSibling(c4)) {
									if (xmlStrEqual(c4->name, xmlCharStrdup("serviceType"))) {
										printf("service type is %s\n", xmlNodeGetContent(c4));
										xmlChar *stype = xmlNodeGetContent(c4);
										if (xmlStrEqual(stype,
													xmlCharStrdup("urn:schemas-upnp-org:service:TmApplicationServer:1"))) {
											type = SERVICE_TYPE_APP;
										} else if (xmlStrEqual(stype,
													xmlCharStrdup("urn:schemas-upnp-org:service:TmClientProfile:1"))) {
											type = SERVICE_TYPE_CLP;
										} else if (xmlStrEqual(stype,
													xmlCharStrdup("urn:schemas-upnp-org:service:TmNotificationServer:1"))) {
											type = SERVICE_TYPE_NOT;
										}
									} else if (xmlStrEqual(c4->name, xmlCharStrdup("SCPDURL"))) {
										str_append(&(server->sinfo[type].surl), xmlNodeGetContent(c4));
										printf("subscribe url is %s\n", server->sinfo[type].surl);
									} else if (xmlStrEqual(c4->name, xmlCharStrdup("controlURL"))) {
										str_append(&(server->sinfo[type].curl), xmlNodeGetContent(c4));
										printf("control url is %s\n", server->sinfo[type].curl);
									} else if (xmlStrEqual(c4->name, xmlCharStrdup("eventSubURL"))) {
										str_append(&(server->sinfo[type].eurl), xmlNodeGetContent(c4));
										printf("event url is %s\n", server->sinfo[type].eurl);
									}
								}
							}
						}
					}
				}
			}
		}
		xmlFreeDoc(doc);
	}
	http_client_free_rsp(rp);
	return server;
}

uint16_t remote_server_get_application_list(struct remote_server *server, uint32_t pid, char *filter)
{
	char buf[100];

	if (!server) {
		return 0;
	}

	sprintf(buf, "<AppListingFilter>%s</AppListingFilter><ProfileID>%d</ProfileID>", filter, pid);
	return remote_server_invoke_action(server, action_map[ACTION_GET_APPLICATION_LIST].stype, action_map[ACTION_GET_APPLICATION_LIST].name, buf, action_map[ACTION_GET_APPLICATION_LIST].handler);
}

uint16_t remote_server_launch_application(struct remote_server *server, uint32_t appid, uint32_t pid)
{
	char buf[100];

	if (!server) {
		return 0;
	}

	sprintf(buf, "<AppID>0x%08x</AppID><ProfileID>%d</ProfileID>", appid, pid);
	return remote_server_invoke_action(server, action_map[ACTION_LAUNCH_APPLICATION].stype, action_map[ACTION_LAUNCH_APPLICATION].name, buf, action_map[ACTION_LAUNCH_APPLICATION].handler);
}

uint16_t remote_server_set_client_profile(struct remote_server *server, uint32_t pid)
{
	str_t param = 0;
	uint16_t ret = 0;
	char *profile = 
		"<ClientProfile>"
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<clientProfile>"
		"<clientID>Cl_1</clientID>"
		"<friendlyName>Client One</friendlyName>"
		"<manufacturer>man_2</manufacturer>"
		"<modelName>CL_Model2</modelName>"
		"<modelNumber>2009</modelNumber>"
		"<iconPreference>"
		"<mimetype>image/png</mimetype>"
		"<width>240</width>"
		"<height>240</height>"
		"<depth>24</depth>"
		"</iconPreference>"
		"<connectivity>"
		"<bluetooth>"
		"<bdAddr>1A2B3C4D5E6F</bdAddr>"
		"<startConnection>false</startConnection>"
		"</bluetooth>"
		"</connectivity>"
		"<rtpStreaming>"
		"<payloadType>0,99</payloadType>"
		"<audioIPL>4800</audioIPL>"
		"<audioMPL>9600</audioMPL>"
		"</rtpStreaming>"
		"<services>"
		"<notification>"
		"<notiUiSupport>true</notiUiSupport>"
		"<maxActions>3</maxActions>"
		"<actionNameMaxLength>15</actionNameMaxLength>"
		"<notiTitleMaxLength>25</notiTitleMaxLength>"
		"<notiBodyMaxLength>100</notiBodyMaxLength>"
		"</notification>"
		"</services>"
		"<mirrorLinkVersion>"
		"<majorVersion>1</majorVersion>"
		"<minorVersion>1</minorVersion>"
		"</mirrorLinkVersion>"
		"<misc>"
		"<driverDistractionSupport>true</driverDistractionSupport>"
		"</misc>"
		"</clientProfile>"
		"</ClientProfile>";
	char buf[50];

	if (!server) {
		return 0;
	}

	sprintf(buf, "<ProfileID>%d</ProfileID>", pid);
	str_append(&param, buf);
	str_append(&param, profile);
	ret = remote_server_invoke_action(server, action_map[ACTION_SET_CLIENT_PROFILE].stype, action_map[ACTION_SET_CLIENT_PROFILE].name, param, action_map[ACTION_SET_CLIENT_PROFILE].handler);
	free(param);
	return ret;
}
uint16_t remote_server_invoke_action(struct remote_server *server, uint16_t stype, char *action, char *args, action_parser handler)
{
	struct http_req *rq = 0;
	struct http_rsp *rp = 0;
	char buf[300] = {0};
	str_t req = 0;
	char *service = 0;
	uint16_t ret = 0;

	switch (stype) {
		case SERVICE_TYPE_APP:
			service = "TmApplicationServer:1";
			break;
		case SERVICE_TYPE_CLP:
			service = "TmClientProfile:1";
			break;
		case SERVICE_TYPE_NOT:
			service = "TmNotificationServer:1";
			break;
	}
	rq = http_client_make_req("POST", server->sinfo[stype].curl);
	http_client_add_header(rq, buf);
	http_client_add_header(rq, "CONTENT-TYPE: text/xml; charset=\"utf-8\"\r\n");
	sprintf(buf, "SOAPACTION: \"urn:schemas-upnp-org:service:%s#%s\"\r\n", service, action);
	http_client_add_header(rq, buf);
	sprintf(buf, "<?xml version=\"1.0\"?>"
			"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
			"<s:Body>"
			"<u:%s xmlns:u=\"urn:schemas-upnp-org:service:%s\">", action, service);
	str_append(&req, buf);
	str_append(&req, args);
	sprintf(buf, "</u:%s>"
			"</s:Body>"
			"</s:Envelope>", action);
	str_append(&req, buf);
	http_client_set_body(rq, req);
	free(req);
	rp = http_client_send(server->ip, server->port, rq);
	if (rp) {
		switch (http_client_get_errcode(rp)) {
			case 200:
				{
					printf("action invoke successfully.\n");
					printf("response body is %s\n", http_client_get_body(rp));
					if (handler) {
						if (handler(server, http_client_get_body(rp))) {
							ret = 200;
						}
					}
				}
				break;
			default:
				printf("action invoke error %d\n", http_client_get_errcode(rp));
				ret = http_client_get_errcode(rp);
				break;
		}
	}
	http_client_free_rsp(rp);
	return ret;
}

uint8_t get_application_list_parse(struct remote_server *server, char *content)
{
	return 0;
}

uint8_t launch_application_parse(struct remote_server *server, char *content)
{
	return 0;
}

uint8_t set_client_profile_parse(struct remote_server *server, char *content)
{
	return 0;
}

void remote_server_destory(struct remote_server *server)
{
	uint8_t i;
	if (!server) {
		return;
	}
	free(server->ip);
	free(server->uuid);
	for (i = 0; i < SERVICE_TYPE_MAX; i++) {
		free(server->sinfo[i].surl);
		free(server->sinfo[i].curl);
		free(server->sinfo[i].eurl);
	}
	free(server);
}

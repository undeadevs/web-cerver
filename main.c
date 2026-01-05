#include <arpa/inet.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_CONN_QUEUE 128

#define DEFAULT_PORT 3000

#define SERVE_DIR_BUF_SIZE 32768
#define BUF_SIZE 104857600

int decode_uri(char *uri_encoded){
	int old_len = strlen(uri_encoded);
	int new_len = 0;
	while(*uri_encoded!='\0'){
		char curr_char = *uri_encoded;
		if(curr_char=='%'){
			char next1 = *(uri_encoded+1);
			char next2 = *(uri_encoded+2);
			if(
				(
				(next1>='0' && next1<='9') ||
				(next1>='A' && next1<='F')
				) &&
				(
				(next2>='0' && next2<='9') ||
				(next2>='A' && next2<='F')
				)
			){
				char decoded_char = next2;
				if(next2>='0' && next2<='9'){
					decoded_char-='0';
				}else if(next2>='A' && next2<='F'){
					decoded_char-='A';
					decoded_char+=10;
				}

				if(next1>='0' && next1<='9'){
					decoded_char+=(next1-'0')*16;
				}else if(next1>='A' && next1<='F'){
					decoded_char += (next1-'A'+10)*16;
				}

				memset(uri_encoded, decoded_char, 1);
				size_t rest_len = strlen(uri_encoded+3);
				memcpy(uri_encoded+1, uri_encoded+3, rest_len);
				memset(uri_encoded+rest_len+1, '\0', 2);
			}
		}
		uri_encoded++;
		new_len += 1;
	}

	return new_len - old_len;
}

void handle_client(int server_fd, char *serve_dir){
	struct sockaddr_in client_addr = {0};
	socklen_t client_addrlen = sizeof(client_addr);
	int client_fd = accept(server_fd, (struct sockaddr*) &client_addr, &client_addrlen);
	if(client_fd == -1){
		perror("Connection failed");
		return;
	}
	pid_t pid = fork();

	if(pid<0) {
		perror("Fork failed");
		close(client_fd);
		return;
	}else if(pid>0) {
		printf("---------------------------------------------------\n");
		return;
	};

	char *arena = malloc(BUF_SIZE*3);
	memset(arena, 0, BUF_SIZE*3);
	size_t arena_top = 0;

	char *req_buf = arena+arena_top;
	arena_top += BUF_SIZE;

	char *client_addr_str = inet_ntoa(client_addr.sin_addr);
	printf("Connected: %s\n", client_addr_str);

	printf("Reading request bytes...\n");
	ssize_t nread = recvfrom(client_fd, req_buf, BUF_SIZE, 0, (struct sockaddr*) &client_addr, &client_addrlen);
	if(nread == -1){
		perror("Error on receiving request bytes");
		free(arena);
		close(client_fd);
		exit(1);
		return;
	}

	printf("Parsing request line...\n");
	size_t req_line_sz = strcspn(req_buf, "\n");
	if(!req_line_sz || req_line_sz==strlen(req_buf)){
		free(arena);
		close(client_fd);
		exit(1);
		return;
	}
	req_line_sz-=1;

	char *req_line = arena+arena_top;
	arena_top += req_line_sz+1;
	strncpy(req_line, req_buf, req_line_sz);
	req_line[req_line_sz] = '\0';

	printf("Getting request method from request line...\n");
	size_t req_method_sz = strcspn(req_line, " "); 
	if(!req_method_sz || req_method_sz==req_line_sz){
		free(arena);
		close(client_fd);
		exit(1);
		return;
	}
	char *req_method = arena+arena_top;
	arena_top += req_method_sz+1;
	strncpy(req_method, req_line, req_method_sz);
	req_method[req_method_sz] = '\0';

	printf("Getting request URI from request line...\n");
	size_t req_uri_sz = strcspn(req_line+req_method_sz+1, " ");
	if(!req_uri_sz || req_uri_sz==strlen(req_line+req_method_sz+1)){
		free(arena);
		close(client_fd);
		exit(1);
		return;
	}
	char *req_uri = arena+arena_top;
	arena_top += req_uri_sz+1;
	strncpy(req_uri, req_line+req_method_sz+1, req_uri_sz);
	req_uri[req_uri_sz] = '\0';

	printf("Getting request HTTP version from request line...\n");
	size_t req_httpver_sz = strcspn(req_line+req_method_sz+req_uri_sz+2, " ");
	if(!req_httpver_sz || req_httpver_sz!=strlen(req_line+req_method_sz+req_uri_sz+2)){
		free(arena);
		close(client_fd);
		exit(1);
		return;
	}
	char *req_httpver = arena+arena_top;
	arena_top += req_httpver_sz+1;
	strncpy(req_httpver, req_line+req_method_sz+req_uri_sz+2, req_httpver_sz);
	req_httpver[req_httpver_sz] = '\0';

	printf("Checking if HTTP...\n");
	if(strncmp(req_httpver, "HTTP", 4)!=0){
		free(arena);
		close(client_fd);
		exit(1);
		return;
	}

	printf("Handling GET request making sure the URI starts with `/`\n");
	if(strncmp(req_uri, "/", 1)==0 && strncmp(req_method, "GET", 3)==0){
		printf("A %s request to `%s`\n", req_method, req_uri);

		char *res = arena+arena_top; 
		arena_top += BUF_SIZE*2;
		size_t res_len = 0;

		if(strcmp(req_uri, "/")==0){
			char *temp_uri = "/index.html";
			req_uri = arena+arena_top;
			arena_top += strlen(temp_uri);
			strcpy(req_uri, temp_uri);
			req_uri_sz = strlen(req_uri);
		} else if(strncmp(req_uri, "/?", 2)==0){
			char temp_uri[strlen(req_uri+1)+1];
			strcpy(temp_uri, req_uri+1);
			sprintf(req_uri+1, "index.html%s", temp_uri);
			req_uri_sz = strlen(req_uri);
		}

		size_t req_path_sz = strcspn(req_uri, "?");
		char *req_path = arena+arena_top;
		arena_top += req_path_sz+1;
		strncpy(req_path, req_uri, req_path_sz);
		req_path[req_path_sz] = '\0';

		size_t req_qparams_str_sz = req_uri_sz-req_path_sz;
		if(req_qparams_str_sz){
			req_qparams_str_sz-=1;
		}
		char *req_qparams_str = arena+arena_top;
		arena_top += req_qparams_str_sz+1;
		if(req_qparams_str_sz){
			strncpy(req_qparams_str, req_uri+req_path_sz+1, req_qparams_str_sz);
		}
		req_qparams_str[req_qparams_str_sz] = '\0';

		decode_uri(req_path);
		decode_uri(req_qparams_str);

		printf("Path: `%s`\n", req_path);
		if(req_qparams_str_sz) printf("Query Params: `%s`\n", req_qparams_str);

		if(strcmp(req_path, "/healthcheck") == 0){
			char res_h[500]; 

			sprintf(
				res_h, 
				"%s 200 OK\r\n"
				"server: Web-Cerver\r\n"
				"connection: Close\r\n"
				"content-length: 2\r\n"
				"\r\n"
				"OK"
				,
				req_httpver
			);

			strncpy(res, res_h, strlen(res_h));

			res_len += strlen(res);
		} else {
			size_t file_path_len = (strlen(serve_dir)+strlen(req_path));
			char file_path[file_path_len+1];

			sprintf(file_path, "%s%s", serve_dir, req_uri);
			file_path[file_path_len] = '\0';

			FILE *fptr = fopen(file_path, "r");

			if(fptr!=NULL){
				printf("Found requested file `%s`\n", file_path);
				fseek(fptr, 0, SEEK_END);
				size_t fsize = ftell(fptr);
				fseek(fptr, 0, SEEK_SET);

				printf("File size: %zu\n", fsize);

				char res_h[500]; 
				sprintf(
					res_h, 
					"%s 200 OK\r\n"
					"server: Web-Cerver\r\n"
					"connection: Close\r\n"
					"content-length: %zu\r\n"
					"\r\n"
					,
					req_httpver,
					fsize
				);

				strncpy(res, res_h, strlen(res_h));

				res_len += strlen(res_h);

				size_t capped_fsize = BUF_SIZE >= fsize ? fsize : BUF_SIZE;
				fread(res+res_len, capped_fsize, 1, fptr);
				res_len+=capped_fsize;

				fseek(fptr, 0, SEEK_SET);
				fclose(fptr);
			}else{
				printf("Unable to find requested file `%s`\n", file_path);
				printf("404 Response\n");
				char *body = "Not Found";

				char res_h[500]; 
				sprintf(
					res_h, 
					"%s 404 NOT FOUND\r\n"
					"server: Web-Cerver\r\n"
					"connection: Close\r\n"
					"content-length: %zu\r\n"
					"\r\n"
					,
					req_httpver,
					strlen(body)
				);

				strncpy(res, res_h, strlen(res_h));

				res_len += strlen(res_h);

				strncpy(res+res_len, body, strlen(body));
				res_len+=strlen(body);
			}
		}

		printf("Sending response...\n");
		send(client_fd, res, res_len, 0);
		printf("Sent.\n");
	}

	free(arena);
	close(client_fd);

	exit(0);
}

int main(int argc, char **argv){
	unsigned int port = DEFAULT_PORT;
	char serve_dir[SERVE_DIR_BUF_SIZE];

	if(argc>1){
		unsigned arg_serve_dir_i = 2;
		port = atoi(argv[1]);
		if(port==0 && *argv[1]!='0'){
			arg_serve_dir_i = 1;
			port = DEFAULT_PORT;
		}

		strcpy(serve_dir, argv[arg_serve_dir_i]);
		if(strlen(serve_dir)>1 && strncmp(serve_dir+strlen(serve_dir)-1, "/", 1)==0){
			memset(serve_dir+strlen(serve_dir)-1, '\0', 1);
		}
	}else{
		strcpy(serve_dir, ".");
	}

	DIR* dir = opendir(serve_dir);
	if (dir) {
		closedir(dir);
	} else if (ENOENT == errno) {
		printf("Directory `%s` doesn't exist\n", serve_dir);
		return 1;
	} else {
		perror("opendir");
	}

	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in server_addr = {0};

	if(server_fd == -1){
		fprintf(stderr, "Socket creation failed\n");
		return 1;
	}

	socklen_t server_addrlen = sizeof(server_addr);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(server_fd, (struct sockaddr*) &server_addr, server_addrlen) == -1){
		perror("Socket binding failed");
		return 1;
	}

	if(listen(server_fd, MAX_CONN_QUEUE) == -1){
		perror("Socket listening failed");
		return 1;
	}

	printf("Listening to port %d\n", port);
	signal(SIGCHLD, SIG_IGN);
	while (1) {
		handle_client(server_fd, serve_dir);
	}

	close(server_fd);
	return 0;
}

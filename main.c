#include <arpa/inet.h>
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

int main(int argc, char **argv){
	unsigned int port = DEFAULT_PORT;
	char *serve_dir = malloc(32768);

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

	char *req_buf = malloc(BUF_SIZE);

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

	pid_t pid = 0;
	while (1) {
		if(pid == 0) printf("---------------------------------------------------\n");

		memset(req_buf, 0, BUF_SIZE);

		struct sockaddr_in client_addr = {0};
		socklen_t client_addrlen = sizeof(client_addr);
		int client_fd = accept(server_fd, (struct sockaddr*) &client_addr, &client_addrlen);
		if(client_fd == -1){
			perror("Connection failed");
			continue;
		}
		pid = fork();

		if(pid<0) {
			perror("Fork failed");
			close(client_fd);
			continue;
		}else if(pid>0) continue;

		char *client_addr_str = inet_ntoa(client_addr.sin_addr);
		printf("Connected: %s\n", client_addr_str);

		printf("Reading request bytes...\n");
		ssize_t nread = recvfrom(client_fd, req_buf, BUF_SIZE, 0, (struct sockaddr*) &client_addr, &client_addrlen);
		if(nread == -1){
			perror("Error on receiving request bytes");
			close(client_fd);
			continue;
		}

		printf("Parsing request line...\n");
		size_t req_line_sz = strcspn(req_buf, "\n");
		if(!req_line_sz){
			close(client_fd);
			continue;
		}
		req_line_sz-=1;

		char *req_line = malloc(req_line_sz*sizeof(char));
		memcpy(req_line, req_buf, req_line_sz);

		printf("Getting request method from request line...\n");
		size_t req_method_sz = strcspn(req_line, " "); 
		if(!req_method_sz){
			close(client_fd);
			free(req_line);
			continue;
		}
		char *req_method = malloc((req_method_sz+1)*sizeof(char));
		memcpy(req_method, req_line, req_method_sz);
		memset(req_method+req_method_sz, '\0', 1);

		printf("Getting request URI from request line...\n");
		size_t req_uri_sz = strcspn(req_line+req_method_sz+1, " ");
		if(!req_uri_sz){
			close(client_fd);
			free(req_method);
			free(req_line);
			continue;
		}
		char *req_uri = malloc((req_uri_sz+1)*sizeof(char));
		memcpy(req_uri, req_line+req_method_sz+1, req_uri_sz);
		memset(req_uri+req_uri_sz, '\0', 1);
		decode_uri(req_uri);

		printf("Getting request HTTP version from request line...\n");
		size_t req_httpver_sz = strcspn(req_line+req_method_sz+req_uri_sz+2, " ");
		if(!req_httpver_sz){
			close(client_fd);
			free(req_uri);
			free(req_method);
			free(req_line);
			continue;
		}
		char *req_httpver = malloc((req_httpver_sz+1)*sizeof(char));
		memcpy(req_httpver, req_line+req_method_sz+req_uri_sz+2, req_httpver_sz);
		memset(req_httpver+req_httpver_sz, '\0', 1);

		printf("Checking if HTTP...\n");
		if(strncmp(req_httpver, "HTTP", 4)!=0){
			close(client_fd);
			free(req_httpver);
			free(req_uri);
			free(req_method);
			free(req_line);
			continue;
		}

		printf("Handling GET request making sure the URI starts with `/`\n");
		if(strncmp(req_uri, "/", 1)==0 && strncmp(req_method, "GET", 3)==0){
			printf("A %s request to `%s`\n", req_method, req_uri);

			char *res = malloc(BUF_SIZE*2); 
			size_t res_len = 0;

			if(strcmp(req_uri, "/")==0){
				strcpy(req_uri, "/index.html");
			}

			size_t file_path_len = (strlen(serve_dir)+strlen(req_uri));
			char *file_path = malloc(file_path_len*sizeof(char)+1);

			sprintf(file_path, "%s%s", serve_dir, req_uri);
			memset(file_path+file_path_len, 0, 1);

			FILE *fptr = fopen(file_path, "r");

			if(fptr!=NULL){
				printf("Found requested file `%s`\n", file_path);
				fseek(fptr, 0, SEEK_END);
				size_t fsize = ftell(fptr);
				fseek(fptr, 0, SEEK_SET);

				printf("File size: %zu\n", fsize);

				char *res_h = malloc(BUF_SIZE); 
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

				memcpy(res, res_h, strlen(res_h));

				res_len += strlen(res_h);

				size_t capped_fsize = BUF_SIZE >= fsize ? fsize : BUF_SIZE;
				fread(res+res_len, capped_fsize, 1, fptr);
				res_len+=capped_fsize;

				fseek(fptr, 0, SEEK_SET);
				fclose(fptr);

				free(res_h);
			}else{
				printf("Unable to find requested file `%s`\n", file_path);
				printf("404 Response\n");
				char *body = "Not Found";

				char *res_h = malloc(BUF_SIZE); 
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

				memcpy(res, res_h, strlen(res_h));

				res_len += strlen(res_h);

				memcpy(res+res_len, body, strlen(body));
				res_len+=strlen(body);

				free(res_h);
			}

			printf("Sending response...\n");
			send(client_fd, res, res_len, 0);
			printf("Sent.\n");

			free(file_path);
			free(res);
		}

		close(client_fd);

		free(req_httpver);
		free(req_uri);
		free(req_method);
		free(req_line);
	}

	free(req_buf);

	close(server_fd);
	return 0;
}

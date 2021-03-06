#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <ncurses.h>
#include <string.h>

 //methods 
void* dashboardThreadController(void *arg);
void* UIThreadController(void *arg);
int getAddress(const char *node, const char *service, struct addrinfo **address);
void getCondition(int fd, struct addrinfo *address);
void getInput(int fd, struct addrinfo *address);
void fillDashboard(int fd, struct addrinfo *address);
int createSocket(void);
void sendCommand(int fd, struct addrinfo *address);


//variables 
int engineInc = 10;
int enginePower = 0;
char *fuel;
char *altitude;
float rcsInc = 0.1;
float rcsRoll = 0;

 
int main(int argc, const char **argv) {
	pthread_t UIThread;
	int user = pthread_create(&UIThread, NULL, UIThreadController, NULL);
    pthread_t dashboardThread;
    int dash = pthread_create(&dashboardThread, NULL, dashboardThreadController, NULL);

    if (user != 0) {
        fprintf(stderr, "ERROR: Thread could not be created");
        exit(-1);
    } 
	if (dash != 0) {
		fprintf(stderr, "ERROR: Thread could not be Created\n");
		exit(-1);
	}
    pthread_join(dashboardThread, NULL);
}
 
 /*
 thread for the dashboard 
 */
void* dashboardThreadController(void *arg) {
	char *landerPort = "65200";
	char *landerHost = "127.0.1.1";
    char *dashPort = "65250";
    char *dashHost = "127.0.1.1";
    struct addrinfo *dashAddress, *landerAddress;
    int dashSocket, landerSocket;

    getAddress(dashHost, dashPort, &dashAddress);
    getAddress(landerHost, landerPort, &landerAddress);
    dashSocket = createSocket();
    landerSocket = createSocket();
    while (1) {
        getCondition(landerSocket, landerAddress);
        fillDashboard(dashSocket, dashAddress);
    }
}

/*
thread for the user input 
*/
void* UIThreadController(void *arg) {
	char *host = "127.0.1.1";
	char *port = "65200";
	struct addrinfo *address;
	int fd;

	getAddress(host, port, &address);
	fd = createSocket();
	getInput(fd, address);
	exit(0);
}

/*
method to detect when the user hits he arrow keys 
and move the lander correspondinly 
*/
void getInput(int fd, struct addrinfo *address) {
	noecho();
    initscr();
	keypad(stdscr, TRUE);
    
    int keyPressed;
    printw("Use Arrow  Button pressed to controller the lander\n");
    printw("UP and DOWN to contoler vertical Thrust, LEFT and RIGHT to control horizontal angle\n");
    printw("ESC to quit");
 
	while ((keyPressed = getch()) != 27) {
		move(10, 0);
		printw("\nFuel: %s \nAltitude: %s", fuel, altitude);
		if (keyPressed == 258 && enginePower >= 10) {
		enginePower -= engineInc;
		sendCommand(fd, address);
		}
		if (keyPressed == 259 && enginePower <= 90) {
			enginePower += engineInc;
			sendCommand(fd, address);
		}
		else if (keyPressed == 260 && rcsRoll > -0.5) {
			rcsRoll -= rcsInc;
			sendCommand(fd, address);
		}
		else if (keyPressed == 261 && rcsRoll <= 0.4) {
			rcsRoll += rcsInc;
			sendCommand(fd, address);
		}
        move(0, 0);
        refresh();
    }
    endwin();

    exit(1);
}
 
/*
method to fill the dashboard with relevent information 
*/
void fillDashboard(int fd, struct addrinfo *address) {
	const size_t buffsize = 4096;
	char outgoing[buffsize];
	snprintf(outgoing, sizeof(outgoing), "fuel: %s \naltitude: %s", fuel, altitude);

	sendto(fd, outgoing, strlen(outgoing), 0, address->ai_addr, address->ai_addrlen);
}

/*
method to send commands to the lander 
*/
void sendCommand(int fd, struct addrinfo *address) {
    const size_t buffsize=4096;
    char outgoing[buffsize];
 
    snprintf(outgoing, sizeof(outgoing), "command:!\nmain-engine: %i\nrcs-roll: %f", enginePower, rcsRoll);
    sendto(fd, outgoing, strlen(outgoing), 0, address->ai_addr, address->ai_addrlen);
}
 
/*
method to get the address of the server 
*/
int getAddress(const char *node, const char *service, struct addrinfo **address) {
    struct addrinfo hints = {
        .ai_flags = 0,
        .ai_family = AF_INET,
        .ai_socktype = SOCK_DGRAM
    };
 
    if(node)
        hints.ai_flags = AI_CANONNAME;
    else
        hints.ai_flags = AI_PASSIVE;
    int err = getaddrinfo(node, service, &hints, address);
    if(err) {
        fprintf(stderr, "ERROR: Could not retrieve address %s\n", gai_strerror(err));
        exit(1);
        return false;
    }
    return true;
}

/*
method to get the condition of the lander 
*/
void getCondition(int fd, struct addrinfo *address) {
	const size_t buffsize = 4096;
	char incoming[buffsize], outgoing[buffsize];
	size_t msgsize;
	strcpy(outgoing, "condition:?");
	sendto(fd, outgoing, strlen(outgoing), 0, address->ai_addr, address->ai_addrlen);
	msgsize = recvfrom(fd, incoming, buffsize, 0, NULL, 0);
	incoming[msgsize] = '\0';
	char *currentCondition = strtok(incoming, ":");
	char *currentConditions[4];
	int i = 0;
	while (currentCondition != NULL) {
		currentConditions[i++] = currentCondition;
		currentCondition = strtok(NULL, ":");
	}
	char *fuel_ = strtok(currentConditions[2], "%");
	fuel = fuel_;
	altitude = strtok(currentConditions[3], "contact");
}
 
/*
method to create a socket fo the TCP protocol 
*/
int createSocket(void) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if(s == -1) {
        fprintf(stderr, "Error:Could not create socket %s\n", strerror(errno));
        exit(1);
        return 0;
    }
    return s;
}
 
/*
method to bind the server to the socket 
*/
int bindSocket(int s, const struct sockaddr *addr, socklen_t addrlen) {
    int err = bind(s, addr, addrlen);
 
    if(err == -1) {
        fprintf(stderr, "Error: could not bind Socket %s\n", strerror(errno));
        return false;
    }
    return true;
}
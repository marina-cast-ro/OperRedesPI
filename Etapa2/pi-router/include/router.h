#ifndef ROUTER_H
#define ROUTER_H

#define MAX_NOMBRE_ROUTER 50
#define MAX_IP            10
#define MAX_VECINOS       10

typedef struct {
    char nombre[MAX_NOMBRE_ROUTER];
    char ip[MAX_IP];
    int puerto;
} Vecino;

typedef struct {
    char routerID[MAX_NOMBRE_ROUTER];
    int puerto;
    int num_vecinos;
    Vecino vecinos[MAX_VECINOS];
} ConfigRouter;

void initRouter(void);

#endif  // ROUTER_H
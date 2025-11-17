#include "BENSCHILLIBOWL.h"

#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

bool IsEmpty(BENSCHILLIBOWL* bcb);
bool IsFull(BENSCHILLIBOWL* bcb);
void AddOrderToBack(Order **orders, Order *order);

MenuItem BENSCHILLIBOWLMenu[] = { 
    "BensChilli", 
    "BensHalfSmoke", 
    "BensHotDog", 
    "BensChilliCheeseFries", 
    "BensShake",
    "BensHotCakes",
    "BensCake",
    "BensHamburger",
    "BensVeggieBurger",
    "BensOnionRings",
};
int BENSCHILLIBOWLMenuLength = 10;

/* Select a random item from the Menu and return it */
MenuItem PickRandomMenuItem() {
    int idx = rand() % BENSCHILLIBOWLMenuLength;
    return BENSCHILLIBOWLMenu[idx];
}

/* Allocate memory for the Restaurant, then create the mutex and condition variables needed to instantiate the Restaurant */

BENSCHILLIBOWL* OpenRestaurant(int max_size, int expected_num_orders) {
    BENSCHILLIBOWL *bcb = (BENSCHILLIBOWL *) malloc(sizeof(BENSCHILLIBOWL));
    if (bcb == NULL) return NULL;

    bcb->orders = NULL;
    bcb->current_size = 0;
    bcb->max_size = max_size;
    bcb->next_order_number = 1;
    bcb->orders_handled = 0;
    bcb->expected_num_orders = expected_num_orders;

    if (pthread_mutex_init(&bcb->mutex, NULL) != 0) {
        free(bcb);
        return NULL;
    }
    if (pthread_cond_init(&bcb->can_add_orders, NULL) != 0) {
        pthread_mutex_destroy(&bcb->mutex);
        free(bcb);
        return NULL;
    }
    if (pthread_cond_init(&bcb->can_get_orders, NULL) != 0) {
        pthread_cond_destroy(&bcb->can_add_orders);
        pthread_mutex_destroy(&bcb->mutex);
        free(bcb);
        return NULL;
    }

    printf("Restaurant is open!\n");
    return bcb;
}


/* check that the number of orders received is equal to the number handled (ie.fullfilled). Remember to deallocate your resources */

void CloseRestaurant(BENSCHILLIBOWL* bcb) {
    if (bcb == NULL) return;

    pthread_mutex_lock(&bcb->mutex);
    /* Wait until all expected orders have been handled */
    while (bcb->orders_handled < bcb->expected_num_orders) {
        pthread_cond_wait(&bcb->can_get_orders, &bcb->mutex);
    }
    pthread_mutex_unlock(&bcb->mutex);

    /* At this point there should be no outstanding orders */
    /* Free any leftover orders (shouldn't be any if counts matched) */
    Order *cur = bcb->orders;
    while (cur) {
        Order *tmp = cur->next;
        free(cur);
        cur = tmp;
    }

    pthread_cond_destroy(&bcb->can_add_orders);
    pthread_cond_destroy(&bcb->can_get_orders);
    pthread_mutex_destroy(&bcb->mutex);

    free(bcb);

    printf("Restaurant is closed!\n");
}

/* add an order to the back of queue */
int AddOrder(BENSCHILLIBOWL* bcb, Order* order) {
    if (bcb == NULL || order == NULL) return -1;

    pthread_mutex_lock(&bcb->mutex);

    /* Wait until not full */
    while (IsFull(bcb)) {
        pthread_cond_wait(&bcb->can_add_orders, &bcb->mutex);
    }

    /* assign order number and add to back */
    order->order_number = bcb->next_order_number++;
    order->next = NULL;
    AddOrderToBack(&bcb->orders, order);

    bcb->current_size++;

    /* Wake up cooks that are waiting for orders */
    pthread_cond_signal(&bcb->can_get_orders);

    int assigned = order->order_number;
    pthread_mutex_unlock(&bcb->mutex);
    return assigned;
}

/* remove an order from the queue */
Order *GetOrder(BENSCHILLIBOWL* bcb) {
    if (bcb == NULL) return NULL;

    pthread_mutex_lock(&bcb->mutex);

    /* Wait while empty and there are still expected orders to be produced/handled */
    while (IsEmpty(bcb) && bcb->orders_handled < bcb->expected_num_orders) {
        pthread_cond_wait(&bcb->can_get_orders, &bcb->mutex);
    }

    /* If empty and all orders handled -> no more orders */
    if (IsEmpty(bcb) && bcb->orders_handled >= bcb->expected_num_orders) {
        pthread_mutex_unlock(&bcb->mutex);
        return NULL;
    }

    /* Pop front order */
    Order *order = bcb->orders;
    if (order != NULL) {
        bcb->orders = order->next;
        order->next = NULL;
        bcb->current_size--;
    }

    /* Signal customers waiting to add */
    pthread_cond_signal(&bcb->can_add_orders);

    pthread_mutex_unlock(&bcb->mutex);
    return order;
}

// Optional helper functions (you can implement if you think they would be useful)
bool IsEmpty(BENSCHILLIBOWL* bcb) {
  return (bcb->current_size == 0);
}

bool IsFull(BENSCHILLIBOWL* bcb) {
  return (bcb->current_size >= bcb->max_size);
}

/* this methods adds order to rear of queue */
void AddOrderToBack(Order **orders, Order *order) {
    if (orders == NULL || order == NULL) return;
    if (*orders == NULL) {
        *orders = order;
        order->next = NULL;
    } else {
        Order *cur = *orders;
        while (cur->next != NULL) cur = cur->next;
        cur->next = order;
        order->next = NULL;
    }
}

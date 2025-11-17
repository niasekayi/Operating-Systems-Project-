#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#include "BENSCHILLIBOWL.h"

// Feel free to play with these numbers! This is a great way to
// test your implementation.
#define BENSCHILLIBOWL_SIZE 100
#define NUM_CUSTOMERS 90
#define NUM_COOKS 10
#define ORDERS_PER_CUSTOMER 3
#define EXPECTED_NUM_ORDERS (NUM_CUSTOMERS * ORDERS_PER_CUSTOMER)

// Global variable for the restaurant.
BENSCHILLIBOWL *bcb;

/**
 * Thread funtion that represents a customer. A customer should:
 *  - allocate space (memory) for an order.
 *  - select a menu item.
 *  - populate the order with their menu item and their customer ID.
 *  - add their order to the restaurant.
 */
void* BENSCHILLIBOWLCustomer(void* tid) {
    int customer_id = (int)(long) tid;
    unsigned int seed = (unsigned int) (time(NULL) ^ (customer_id << 16));
    for (int i = 0; i < ORDERS_PER_CUSTOMER; ++i) {
        Order *order = (Order *) malloc(sizeof(Order));
        if (order == NULL) {
            fprintf(stderr, "Customer %d: out of memory\n", customer_id);
            continue;
        }
        /* pick random menu item - use thread-local rand_r to vary */
        int idx = rand_r(&seed) % BENSCHILLIBOWLMenuLength;
        order->menu_item = BENSCHILLIBOWLMenu[idx];
        order->customer_id = customer_id;
        order->next = NULL;

        int order_num = AddOrder(bcb, order);
        if (order_num < 0) {
            /* failed to add - free and continue */
            free(order);
        } else {
            /* Optionally, customers could print; not required for autograder */
            // printf("Customer %d placed order %d: %s\n", customer_id, order_num, order->menu_item);
        }

        /* small random sleep to mix timing */
        usleep((rand_r(&seed) % 100) * 1000);
    }
    return NULL;
}

/**
 * Thread function that represents a cook in the restaurant. A cook should:
 *  - get an order from the restaurant.
 *  - if the order is valid, it should fulfill the order, and then
 *    free the space taken by the order.
 * The cook should take orders from the restaurants until it does not
 * receive an order.
 */
void* BENSCHILLIBOWLCook(void* tid) {
    int cook_id = (int)(long) tid;
    int orders_fulfilled = 0;
    unsigned int seed = (unsigned int) (time(NULL) ^ (cook_id << 8));

    while (1) {
        Order *order = GetOrder(bcb);
        if (order == NULL) {
            /* No more orders left */
            break;
        }

        /* "Fulfill" the order: simulate work */
        usleep((rand_r(&seed) % 100) * 1000);

        /* After fulfilling, increment shared orders_handled */
        pthread_mutex_lock(&bcb->mutex);
        bcb->orders_handled += 1;
        /* If we've fulfilled all expected orders, wake up any waiting cooks */
        if (bcb->orders_handled >= bcb->expected_num_orders) {
            pthread_cond_broadcast(&bcb->can_get_orders);
        }
        pthread_mutex_unlock(&bcb->mutex);

        /* free the order */
        free(order);
        orders_fulfilled++;
    }

    printf("Cook #%d fulfilled %d orders\n", cook_id, orders_fulfilled);
    return NULL;
}

/**
 * Runs when the program begins executing. This program should:
 *  - open the restaurant
 *  - create customers and cooks
 *  - wait for all customers and cooks to be done
 *  - close the restaurant.
 */
int main() {
    srand(time(NULL));
    bcb = OpenRestaurant(BENSCHILLIBOWL_SIZE, EXPECTED_NUM_ORDERS);
    if (bcb == NULL) {
        fprintf(stderr, "Failed to open restaurant\n");
        return 1;
    }

    pthread_t customers[NUM_CUSTOMERS];
    pthread_t cooks[NUM_COOKS];

    /* create cook threads */
    for (long i = 0; i < NUM_COOKS; ++i) {
        if (pthread_create(&cooks[i], NULL, BENSCHILLIBOWLCook, (void*) i) != 0) {
            fprintf(stderr, "Failed to create cook %ld\n", i);
            return 1;
        }
    }

    /* create customer threads */
    for (long i = 0; i < NUM_CUSTOMERS; ++i) {
        if (pthread_create(&customers[i], NULL, BENSCHILLIBOWLCustomer, (void*) i) != 0) {
            fprintf(stderr, "Failed to create customer %ld\n", i);
            return 1;
        }
    }

    /* join customers */
    for (int i = 0; i < NUM_CUSTOMERS; ++i) {
        pthread_join(customers[i], NULL);
    }

    /* At this point all customers have placed their orders. Signal cooks in case they are waiting */
    pthread_mutex_lock(&bcb->mutex);
    pthread_cond_broadcast(&bcb->can_get_orders);
    pthread_mutex_unlock(&bcb->mutex);

    /* join cooks */
    for (int i = 0; i < NUM_COOKS; ++i) {
        pthread_join(cooks[i], NULL);
    }

    CloseRestaurant(bcb);
    return 0;
}

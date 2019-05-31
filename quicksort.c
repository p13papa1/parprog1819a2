#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>


#define N 1000000       // μέγεθος της ουράς μηνυμάτων
#define THREADS 4       // μεγεθος της ομάδας των threads
#define SIZE 100        // μέγεθος που πρέπει να ταξινομηθεί
#define THRESHOLD 10    // μέγεθος του threshold 

//τύποι μηνυμάτων
#define WORK 0
#define DONE 1
#define SHUTDOWN 2

struct message {
    int type;
    int start;
    int end;
};

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t msg_in = PTHREAD_COND_INITIALIZER;
pthread_cond_t msg_out = PTHREAD_COND_INITIALIZER;

struct message mqueue[N];
int q_input = 0, q_output = 0;
int m_count = 0;

void send(int type, int start, int end) {
    pthread_mutex_lock(&mutex);
    while (m_count >= N) {
        printf("\nProducer locked\n");
        pthread_cond_wait(&msg_out, &mutex);
    }
    
    
    mqueue[q_input].type = type;
    mqueue[q_input].start = start;
    mqueue[q_input].end = end;
    q_input = (q_input + 1) % N;
    m_count++;

    pthread_cond_signal(&msg_in);
    pthread_mutex_unlock(&mutex);
}

void recv(int *type, int *start, int *end) {
    pthread_mutex_lock(&mutex);
    while (m_count < 1) {
        printf("\nConsumer locked\n");
        pthread_cond_wait(&msg_in, &mutex);
    }

    
    *type = mqueue[q_output].type;
    *start = mqueue[q_output].start;
    *end = mqueue[q_output].end;
    q_output = (q_output + 1) % N;
    m_count--;

    pthread_cond_signal(&msg_out);
    pthread_mutex_unlock(&mutex);
}

void swap(double *a, double *b) {
    double tmp = *a;
    *a = *b;
    *b = tmp;
} 

int partition(double *a, int n) {
    int first = 0;
    int middle = n/2;
    int last = n-1;
    if (a[first] > a[middle]) {
        swap(a+first, a+middle);
    }
    if (a[middle] > a[last]) {
        swap(a+middle, a+last);
    }
    if (a[first] > a[middle]) {
        swap(a+first, a+middle);
    }
    double p = a[middle];
    int i, j;
    for (i=1, j=n-2;; i++, j--) {
        while (a[i] < p) i++;
        while (a[j] > p) j--;
        if (i>=j) break;
        swap(a+i, a+j);
    }
    return i;
}

void ins_sort(double *a, int n) {
    int i, j;
    for (i=1; i<n; i++) {
        j = i;
        while (j>0 && a[j-1] > a[j]) {
            swap(a+j, a+j-1);
            j--;    
        }
    }
}

void *thread_func(void *params) {
    double *a = (double*) params;
    
    // έλεγχος για μηνύματα
    int t, s, e;
    recv(&t, &s, &e);
    while (t != SHUTDOWN) {
        if (t == DONE) {
            // προώθηση των μηνυμάτων DONE
            send(DONE, s, e);
        } else if (t == WORK) {
            if (e-s <= THRESHOLD) {
                // αν το μέγεθος είναι μικρό, ταξινόμησε με insertion sort
                ins_sort(a+s, e-s);
                // Στέλνει μήνυμα DONE για το εύρος των δεικτών που έχει ταξινομηθεί
                send(DONE, s, e);
            } else {
                // Αν το μέγεθος του πίνακα δεν είναι μικρό, το χωρίζουμε και εισάγουμε και τα 2 μέρη στην ουρά
                int p = partition(a+s, e-s);
                send(WORK, s, s+p);
                send(WORK, s+p, e);
            }
        }
      
        recv(&t, &s, &e);
    }
    //Αν λάβουμε SHUTDOWN, το εισάγουμε στην ουρά και κάνουμε έξοδο
    send(SHUTDOWN, 0, 0);
    printf("done!\n");
    pthread_exit(NULL);
}

int main() {
    double *a = (double*) malloc(sizeof(double) * SIZE);
    if (a == NULL) {
        printf("Error during memory allocation\n");
        exit(1);
    }

    //Αρχικοποιήστε τον πίνακα με τιμές στο εύρος [0, 1]
    for (int i=0; i<SIZE; i++) {
        a[i] = (double) rand()/RAND_MAX;
    }

    // δημιουργία ομάδας thread με παράμετρο τον πίνακα a
    pthread_t threads[THREADS];
    for (int i=0; i<THREADS; i++){
        if (pthread_create(&threads[i], NULL, thread_func, a) != 0) {
            printf("Thread creation error\n");
            free(a);
            exit(1);
        }
    }

    // στέλνει το αρχικό workload
    send(WORK, 0, SIZE);

    int t, s, e;
    int count = 0;
    recv(&t, &s, &e);
    while (1) {
        if (t == DONE) {
            //Μετράει τον αριθμό των στοιχείων που έχουν ταξινομηθεί με επιτυχία
            count += e-s;
            printf("Done with %d out of %d\n", count, SIZE);
            printf("Partition done: (%d, %d)\n", s, e);
            if (count == SIZE) {
                // Σταματάει να μετράει για μηνύματα από την στιγμή που έχουν ταξινομηθεί όλα τα στοιχεία
                break;
            }
        } else {
            
            send(t, s, e);
        }
        
        recv(&t, &s, &e);
    }
    // όλα τα στοιχεία έχουν ταξινομηθεί, σταματάνε τα threads
    send(SHUTDOWN, 0, 0);

    // αναμονή μεχρι ολα τα threads να εξέλθουν
    for (int i=0; i<THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // έλεγχος για σφάλματα
    int i;
    for (i=0; i<SIZE-1; i++) {
        if (a[i] > a[i+1]) {
            printf("Error! Array is not sorted. a[%d] = %lf, a[%d] = %lf\n", i, a[i], i+1, a[i+1]);
            break;
        }
    }
    if (i == SIZE-1) {
        printf("Sucess!\n");
    }

  
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&msg_in);
    pthread_cond_destroy(&msg_out);
    free(a);
    return 0;
}

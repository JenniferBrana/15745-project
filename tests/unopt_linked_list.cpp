#include <gem5/m5ops.h>
#include <cstdlib>
#include <stdio.h>
#include <string.h>

#define GEM5

// 65536*8 is 512kB.
const int N = 65536 * 2;

template<class T>
class LList{	
public:
    struct node {
        T data;
        node* next;

        node() : data(), next(NULL) { }

        node(T dataIn) : data(dataIn), next(NULL) { }

        ~node() {
            delete next;
        }

        node(const node& copy) {
            data = copy.data;
            next = copy.next;
        }

        node& operator=(const node& assign) {
            if(this != &assign) {
                this->data = assign.data;
                this->next = assign.next;
            }
            return *this;
        }

        node* operator=(const node* assign) {
            if (this != (void *)&assign) {
                this->data = assign->data;
                this->next = assign->next;
            }
            return *this;
        }

        bool operator!=(const node* rhs) {
            return this != (void *)&rhs;
        }
    };

    node* head;

    LList() : head(NULL) {}
    ~LList() {
        delete head;
    }

    LList(const LList<T>& copy) : head(NULL) {
        node* curr = copy.head;
        while(curr != NULL) {
            insert(curr->data);
            curr = curr->next;
        }

    }
	
    LList<T>& operator=(const LList<T>& assign) {
        if(this != &assign) {
            node* curr = assign.head;
            while(curr != NULL) {
                this->insert(curr->data);
                curr = curr->next;
            }
        }
        return *this;
    }

    void insert (T value) {
        node* temp = new node(value);

        if (empty()) {
            head = temp;
        } else {
            node* prev =  NULL;
            node* curr = head;
            while (curr != NULL) {
                prev = curr;
                curr = curr->next;
            }
            prev -> next = temp;
        }
    }

    void push_front(T value) {
        node* temp = new node(value);
        temp -> next = head;
        head = temp;
    }

    bool remove (T target) {
        node* temp = new node(); node* prev = new node(); node* curr = new node();

        if (empty ()) {
            return (-1);
        } else if (target == head -> data) {
            temp = head;
            head = head -> next;
            free (temp);
            return true;
        } else {
            prev = head;
            curr = head -> next;

            while (curr != NULL && curr -> data != target) {
                prev = curr;
                curr = curr -> next;
            }
            
            if (curr != NULL) {
                temp = curr;
                prev -> next = curr -> next;
                free(temp);
                return true;
            } else {
                return false;
            }
        }
    }
		
    bool pop_front() {
        if (empty ()) {
            return (-1);
        } else {
            node* temp = head;
            head = head -> next;
            free (temp);
            return true;
        }
    }
		
    T front() const {
        return head->data;
    }

    bool empty () const {
        return head == NULL;
    }

    bool contains(const T& searchVal) const {
        if (empty()) {
            return false;
        } else {
            node* prev =  NULL;
            node* curr = head;
            while (curr != NULL && curr -> data != searchVal) {
                prev = curr;
                curr = curr -> next;
            }

            return curr != NULL;
        }
    }

};

int sum(LList<int> l) {
    LList<int>::node* node = l.head;
    int total = 0;
    while (node) {
        total += node->data;
        node = node->next;
    }
    return total;
}

int sum2() {
    int total = 0;
    for (int i = 0; i < N; ++i) {
        total += 10;
    }
    return total;
}


int main() {
    LList<int> nums = LList<int>();
    int remaining = N;
    while (remaining > 0) {
        nums.push_front(remaining);
        --remaining;
    }
    m5_reset_stats(0, 0);
    printf("List of numbers 1 .. %d sums to %d\n", N, sum(nums));
    printf("Sum of argv lengths = %d\n", sum2());
    return 0;
}
#include "../lib/mls.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

void my_recursive_free(int m) {
    int p, *d;
    printf("my_recursive_free calling for %d\n", m);
    for(p=-1; m_next(m, &p, &d); ) {
        if (*d > 0) {
            m_free(*d);
        }
    }
}

void test_custom_circular() {
    printf("Testing custom circular reference with 255 marker...\n");
    int hdl = m_reg_freefn(0, my_recursive_free);
    int l1 = m_alloc(10, sizeof(int), hdl);
    m_put(l1, &l1); 
    
    m_free(l1);
    
    assert(m_is_freed(l1));
    printf("Custom circular reference test passed.\n");
}

int main() {
    trace_level = 1;
    m_init();
    
    test_custom_circular();
    
    m_destruct();
    return 0;
}

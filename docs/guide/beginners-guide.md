# Beginners Guide: Thinking in Handles (The Pythonic Way)

If you're coming from Python, JavaScript, or Java, C pointers feel like a nightmare. You have to track addresses, worry about `NULL`, and remember to `free()` everything in the right order.

**MLS changes the game.** In MLS, you don't use pointers. You use **Handles**. Think of a handle as an ID for an object.

---

## The "Pythonic" Example: A Student Database

Let's build a simple program to manage students. Each student has a **name**, an **age**, and a **list of grades**.

### 1. The Pointer Way (Standard C)
*Warning: This code is simplified but still dangerous!*
```c
struct Student {
    char *name;
    int age;
    int *grades;
    int grade_count;
};

// You have to manually malloc the struct, then the name, then the grades...
// And God help you if you forget to free them in the exact reverse order.
```

### 2. The MLS Way (Python-like)
In MLS, everything is a handle. We don't need a `struct`.

```c
#include "mls.h"

int create_student(const char *name, int age) {
    // Create a list to hold student data
    // Index 0: Name (handle)
    // Index 1: Age (raw int)
    // Index 2: Grades (handle)
    int student = m_alloc(3, sizeof(int), MFREE_EACH);

    int name_h = s_printf(0, 0, "%s", name);
    int grades_h = m_alloc(0, sizeof(int), MFREE);

    m_put(student, &name_h);   // Add name handle
    m_put(student, &age);      // Add age raw
    m_put(student, &grades_h); // Add grades handle

    return student;
}

void add_grade(int student, int grade) {
    int grades_h = INT(student, 2);
    m_put(grades_h, &grade);
}

void print_student(int student) {
    printf("Student: %s, Age: %d
", STR(INT(student, 0), 0), INT(student, 1));
}
```

---

## Why this is easier for Beginners:

1.  **One Handle to Rule Them All:** You only need to keep track of the `student` handle.
2.  **Atomic Cleanup:** When you're done, call `m_free(student)`. Because we used `MFREE_EACH`, MLS automatically frees the name string and the grades list for you.
3.  **No Pointer Math:** You never write `*` or `->`. You just ask the handle for what you want.

---

## Current Weaknesses (The "Future" of MLS)

To make C truly feel like Python, we've identified some areas where MLS still requires "C-brain" thinking:

### 1. Manual Handle Management
In Python, variables just "disappear" when you're done. In MLS, you still have to call `m_free()`. 
*Future Goal: Scoped auto-freeing or a simple garbage collector wrapper.*

### 2. Lack of Type Tags
A handle is just an `int`. You can accidentally try to use a "grades list" handle as a "name string" handle.
*Future Goal: Type-tagged handles that warn you if you use a list-handle in a string-function.*

### 3. Named Fields (Dictionary Support)
In the example above, we had to remember that "Age" is at Index 1. In Python, you'd just say `student['age']`.
*Future Goal: A first-class `m_dict` type for named key-value access.*

### 4. Explicit `m_init()`
Beginners often forget to initialize the library.
*Future Goal: Auto-initialization on the first `m_alloc` call.*

---

## Conclusion

MLS is a massive leap forward for C beginners. It provides a safety net that raw pointers simply cannot. By focusing on **Handles**, you can build complex, nested programs today while we work on making the "Python-for-C" experience even smoother.

---

[Back to Home](README.md)

#include <iostream>
#include <vector>
#include <string>
#include "uapi_mm.h"

typedef struct emp_ {
    char name[32];
    uint32_t emp_id;
} emp_t;

typedef struct student_ {
    char name[32];
    uint32_t rollno;
    uint32_t marks_phys;
    uint32_t marks_chem;
    uint32_t marks_maths;
    struct student_ *next;
} student_t;

class Dean {
public:
    std::string name;
    uint32_t establish_year;
    std::vector<std::string> awards;
};

int main() {

    std::string buffer_input;

    mm_init();
    MM_REG_STRUCT(emp_);
    MM_REG_STRUCT(student_);
    MM_REG_STRUCT(Dean);
    mm_print_registered_structure_families();
    
    emp_t *emp1 = (emp_t*)XCALLOC(1, emp_);
    emp_t *emp2 = (emp_t*)XCALLOC(2, emp_);
    emp_t *emp3 = (emp_t*)XCALLOC(3, emp_);

    student_t *stu1 = (student_t*)XCALLOC(1, student_);
    student_t *stu2 = (student_t*)XCALLOC(2, student_);

    Dean *dean1 = (Dean*)XCALLOC(1, Dean);
    Dean *dean2 = (Dean*)XCALLOC(2, Dean);

    std::cout << "Scenario 1" << std::endl;
    mm_print_memory_usage();
    mm_print_block_usage();

    std::cout << "Buffer to the next stage: ";
    getline(std::cin, buffer_input);

    XFREE(emp1);
    XFREE(emp3);
    XFREE(stu2);
    XFREE(dean1);

    std::cout << "Scenario 2" << std::endl;
    mm_print_memory_usage();
    mm_print_block_usage();

    std::cout << "Buffer to the next stage: ";
    getline(std::cin, buffer_input);

    XFREE(emp2);
    XFREE(stu1);

    std::cout << "Scenario 3" << std::endl;
    mm_print_memory_usage();
    mm_print_block_usage();

    std::cout << "Buffer to the next stage: ";
    getline(std::cin, buffer_input);

    XFREE(dean2);

    std::cout << "Scenario 4" << std::endl;
    mm_print_memory_usage();
    mm_print_block_usage();

    // const uint32_t LOOP_TIME = 100;
    // for (uint32_t i = 0; i < LOOP_TIME; i++) {
    //     XCALLOC(1, emp_);
    //     XCALLOC(1, student_);
    // }

    // std::cout << "Print any bottom to check out the summary: ";
    // std::string buffer_input;
    // getline(std::cin, buffer_input);
    // mm_print_memory_usage();
    // mm_print_block_usage();
    return 0;
}
#include "../src/control/limits.hpp"
#include <cassert>
#include <iostream>

/**
 * @brief Test limits functionality per technical specification
 * 
 * Verifies exact clamping behavior as specified in task_detailed.txt
 */
int main() {
    std::cout << "Testing Limits functionality..." << std::endl;
    
    Limits L;
    
    // Test exact assertions from specification
    assert(L.clamp(-3.0) == -2.0);
    assert(L.clamp( 3.0) ==  2.0);
    assert(L.clamp( 0.5) ==  0.5);
    
    // Additional boundary tests
    assert(L.clamp(-2.0) == -2.0);  // Exact minimum
    assert(L.clamp( 2.0) ==  2.0);  // Exact maximum
    assert(L.clamp( 0.0) ==  0.0);  // Zero
    assert(L.clamp(-1.5) == -1.5);  // Within range (negative)
    assert(L.clamp( 1.5) ==  1.5);  // Within range (positive)
    
    // Test extreme values
    assert(L.clamp(-100.0) == -2.0);
    assert(L.clamp( 100.0) ==  2.0);
    
    std::cout << "✅ All Limits tests passed!" << std::endl;
    return 0;
}


#include <memory>
#include <vector>
#include <cstddef>

/**
 * @brief ChunkAllocator is a basic linear allocator, implemented as a set of equal sized
 * pages which can be filled with type T. This allocator is designed to allow for high levels
 * of memory re-use, and to preserve pointer values through execution
 * @note pointers created by this allocator WILL NOT BE invalidated after calling the appropraite
 * methods, but it does open them up for re-use!
 * @note this will always use atleast `sizeof(T) * items_per_page` bytes  of RAM at a minimum! 
 */
template <typename T>
class ChunkAllocator {
public:
    /**
     * @brief basic constructor for the ChunkAllocator
     * @param items_per_page is the number of continuous elements you want in each page before another
     * allocation is needed. Each page will be `sizeof(T) * items_per_page`
     */
    ChunkAllocator (size_t items_per_page = 128) : items_in_page(items_per_page) {
        pages.push_back(std::make_unique<T[]>(items_in_page));
    }

    /**
     * @brief the default destructor of std::vector will invoke the default destructor of the
     * unique ptrs to each page, releasing all memory used
     * @todo maybe this should be private?
     */
    ~ChunkAllocator() = default;

    /**
     * @brief the copy constructor for the ChunkAllocator is explictly deleted to ensure that
     * the memory doesnt get accidentally messed up
     */
    ChunkAllocator (const ChunkAllocator& other) = delete; //No copies for us!

    /**
     * @brief the move constructor should be safe, since we are transfering ownership of all
     * internal resources, but care should still be taken when using it!
     */
    ChunkAllocator (ChunkAllocator&& other) = default; //Only moves :)

    /**
     * @brief allocates an item of type T with its default constructor (must be visible!)
     * @return a `T*` with the newly created item
     * @note This pointer will survive across multiple calls to allocate, however once
     * `reset()` is called, the object is marked to be overwritten!
     */
    constexpr T* allocate() {
        //Check to see if we need to get a new page
        if (item_index >= items_in_page) {
            current_page++;
            item_index = 0;
            if (current_page >= pages.size()) {
                pages.push_back(std::make_unique<T[]>(items_in_page));
            }
        }

        //Get the value to return, increment item index
        T* ret_val = &pages[current_page][item_index];
        item_index++;
        return ret_val;
    }

    /**
     * @brief resets the current write point for new allocs, marking all allocated objects
     * as overwritable
     */
    constexpr void reset() {
        current_page = 0;
        item_index = 0;
    }

private:
    std::vector<std::unique_ptr<T[]>> pages;
    const size_t items_in_page;
    size_t current_page;
    size_t item_index;
};
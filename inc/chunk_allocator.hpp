#pragma once

#include <memory>
#include <vector>
#include <cstddef>
#include <algorithm>
#include <execution>
#include <array>
#include <concepts>

namespace FusionSystem {

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
    ChunkAllocator (size_t items_per_page = 256) : items_in_page(items_per_page), current_page(0), item_index(0) {
        pages.push_back(std::make_unique<std::byte[]>(sizeof(T) * items_in_page));
    }

    /**
     * @brief the default destructor of std::vector will clean up itself, but since each
     * page has manual management, we need to clean it up
     */
    ~ChunkAllocator() {
        reset();
    };

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
     * `reset()` is called, the object is marked to be overwritten, but its DESTRUCTOR IS NEVER CALLED!!
     */
    template <typename... Args>
    constexpr T* allocate(Args... args) {
        //Check to see if we need to get a new page
        if (item_index >= items_in_page) {
            current_page++;
            item_index = 0;
            if (current_page >= pages.size()) {
                pages.push_back(std::make_unique<std::byte[]>(sizeof(T) * items_in_page));
            }
        }

        //Create an instance of that object in location
        std::byte* raw_ptr = pages[current_page].get();
        T* typed_ptr_base = reinterpret_cast<T*>(raw_ptr);
        T* slot = &typed_ptr_base[item_index];
        new(slot) T(std::forward<Args>(args)...);

        //Increment the item creation index
        item_index++;

        //Return the type covered pointer
        return slot;
    }

    /**
     * @brief resets the current write point for new allocs, marking all allocated objects
     * as overwritable
     * @note if items are trivially destructable, this operation is MUCH faster
     */
    constexpr void reset() {
        if constexpr (std::is_trivially_destructible<T>()) {
            current_page = 0;
            item_index = 0;
        } else {
            //Use destory n to call the destructor across blocks of objects
            for (size_t i = 0; i < current_page; ++i) {
                // Cast byte* -> T* so destroy_n knows what to call
                std::destroy_n(reinterpret_cast<T*>(pages[i].get()), items_in_page);
            }

            if (current_page < pages.size()) { // Fixed 'blocks' -> 'pages'
                std::destroy_n(reinterpret_cast<T*>(pages[current_page].get()), item_index);
            }
            current_page = 0;
            item_index = 0;
        }
        
    }

    /**
     * @brief gets the number of items currently in use
     */
    constexpr size_t count() noexcept {
        return (current_page * items_in_page) + item_index + 1;
    }

    /**
     * @brief gets the maximum number of items that can be stored in total reserved memory
     */
    constexpr size_t max() noexcept {
        return (current_page + 1) * items_in_page;
    }

private:
    std::vector<std::unique_ptr<std::byte[]>> pages;
    const size_t items_in_page;
    size_t current_page;
    size_t item_index;
};

}; //End namespace fusion System
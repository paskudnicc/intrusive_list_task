#pragma once

#include <cstdlib>
#include <type_traits>
#include <set>
#include <bits/unique_ptr.h>

namespace intrusive {
    /*
    Тег по-умолчанию чтобы пользователям не нужно было
    придумывать теги, если они используют лишь одну базу
    list_element.
    */
    struct default_tag;

    template<typename Tag = default_tag>
    struct list_element {
        list_element *prev;
        list_element *next;

        explicit list_element(list_element *p = nullptr, list_element *n = nullptr) : prev(p), next(n) {}

        ~list_element() {
            unlink();
        }

        /* Отвязывает элемент из списка в котором он находится. */
        void unlink() {
            if (prev != nullptr) {
                prev->next = next;
            }
            if (next != nullptr) {
                next->prev = prev;
            }
        }
    };

    template<typename T, typename Tag = default_tag>
    struct list;

    template<typename T, typename Tag>
    class list_iterator {
        friend class list<T, Tag>;

        friend class list<std::remove_const_t<T>, Tag>;

        friend class list_iterator<const T, Tag>;

        friend class list_iterator<std::remove_const_t<T>, Tag>;

        /*
        Хранить list_element_base*, а не T* важно.
        Иначе нельзя будет создать list_iterator для
        end().
        */
        list_element<Tag> *current;
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = std::remove_const_t<T>;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type *;
        using reference = value_type &;

        list_iterator() = default;

        list_iterator(list_iterator const &other) : current(other.current) {}

        template<typename NonConstIterator>
        list_iterator(NonConstIterator other, std::enable_if_t<
                std::is_same_v<NonConstIterator, list_iterator<std::remove_const_t<T>, Tag>> &&
                std::is_const_v<T>> * = nullptr) noexcept
                : current(other.current) {}

        T &operator*() const noexcept {
            return static_cast<T &>(*current);
        }

        T *operator->() const noexcept {
            return static_cast<T *>(current);
        }

        list_iterator &operator++() & noexcept {
            if (current != nullptr) {
                current = current->next;
            }
            return *this;
        }

        list_iterator &operator--() & noexcept {
            if (current != nullptr) {
                current = current->prev;
            }
            return *this;
        }

        const list_iterator operator++(int) & noexcept {
            auto tmp = *this;
            ++*this;
            return tmp;
        }

        const list_iterator operator--(int) & noexcept {
            auto tmp = *this;
            --*this;
            return tmp;
        }

        bool operator==(list_iterator const &rhs) const & noexcept {
            return current == rhs.current;
        }

        bool operator!=(list_iterator const &rhs) const & noexcept {
            return current != rhs.current;
        }

    private:
        /*
        Это важно иметь этот конструктор private, чтобы итератор нельзя было создать
        от nullptr.
        */
        explicit list_iterator(list_element<Tag> *cur) noexcept: current(cur) {}

        explicit list_iterator(const list_element<Tag> *cur) noexcept: current(const_cast<list_element<Tag> *>(cur)) {}
    };

    template<typename T, typename Tag>
    struct list {
        typedef list_iterator<T, Tag> iterator;
        typedef list_iterator<const T, Tag> const_iterator;

        static_assert(std::is_convertible_v<T &, list_element<Tag> &>,
                      "value type is not convertible to list_element");

        list() noexcept {}

        list(list const &) = delete;

        list(list &&other) noexcept {
            operator=(std::move(other));
        }

        ~list() {}

        list &operator=(list const &) = delete;

        list &operator=(list &&other) noexcept {
            first = other.first;
            after_last.prev = other.after_last.prev;
            if (after_last.prev != nullptr) {
                after_last.prev->next = &after_last;
            }
            other.after_last.prev = other.first = nullptr;
            return *this;
        }

        void clear() noexcept {
            first = after_last.prev = nullptr;
        }

        list_element<Tag> *first = nullptr;
        list_element<Tag> after_last{};

        /*
        Поскольку вставка изменяет данные в list_element
        мы принимаем неконстантный T&.
        */

        void push_back(T &elem) noexcept {
            auto *p = static_cast<list_element<Tag> *>(&elem);
            if (after_last.prev != nullptr) {
                after_last.prev->next = p;
                p->prev = after_last.prev;
            }
            after_last.prev = p;
            p->next = &after_last;

            if (first == nullptr) {
                first = after_last.prev;
            }
        }

        void pop_back() noexcept {
            if (after_last.prev != nullptr && after_last.prev->prev != nullptr) {
                after_last.prev->prev->next = &after_last;
                after_last.prev = after_last.prev->prev;
            } else {
                clear();
            }
        }

        T &back() noexcept {
            return static_cast<T &>(*after_last.prev);
        }

        T const &back() const noexcept {
            return static_cast<const T &>(*after_last.prev);
        }

        void push_front(T &elem) noexcept {
            auto *p = static_cast<list_element<Tag> *>(&elem);
            p->prev = nullptr;
            if (first != nullptr) {
                first->prev = p;
                p->next = first;
            }
            first = p;
            if (after_last.prev == nullptr) {
                after_last.prev = first;
            }
        }

        void pop_front() noexcept {
            if (first != nullptr && first->next != nullptr) {
                first->next->prev = nullptr;
                first = first->next;
            } else {
                clear();
            }
        }

        T &front() noexcept {
            return static_cast<T &>(*first);
        }

        T const &front() const noexcept {
            return static_cast<const T &>(*first);
        }

        [[nodiscard]] bool empty() const noexcept {
            return first == nullptr;
        }

        iterator begin() noexcept {
            if (first == nullptr) {
                return end();
            }
            return iterator(first);
        }

        const_iterator begin() const noexcept {
            if (first == nullptr) {
                return end();
            }
            return const_iterator(first);
        }

        iterator end() noexcept {
            return iterator(&after_last);
        }

        const_iterator end() const noexcept {
            return const_iterator(&after_last);
        }

        iterator insert(const_iterator pos, T &elem) noexcept {
            auto *new_el = static_cast<list_element<Tag> *>(&elem);
            auto *p = static_cast<list_element<Tag> *>(pos.current);
            if (p->prev != nullptr) {
                p->prev->next = new_el;
            } else {
                first = new_el;
            }
            new_el->prev = p->prev;
            p->prev = new_el;
            new_el->next = p;
            return iterator(new_el);
        }

        iterator erase(const_iterator pos) noexcept {
            if (pos != end()) {
                auto *p = static_cast<list_element<Tag> *>(pos.current);
                p->next->prev = p->prev;
                if (p->prev != nullptr) {
                    p->prev->next = p->next;
                } else {
                    first = p->next;
                }
                return iterator(p->next);
            }
            return end();
        }

        void splice(const_iterator it0, list &donor, const_iterator it1, const_iterator it2) noexcept {
//            if it2 < it1 UB
            if (it1 == it2) {
                return;
            }
            auto *after_incision = static_cast<list_element<Tag> *>(it0.current);
            auto *from = static_cast<list_element<Tag> *>(it1.current);
            auto *to = static_cast<list_element<Tag> *>(it2.current);
            if (from->prev != nullptr) {
                from->prev->next = to;
            } else {
                if (it2 != donor.end()) {
                    donor.first = to;
                } else {
                    donor.first = nullptr;
                }
            }
            if (after_incision->prev != nullptr) {
                after_incision->prev->next = from;
            } else {
                first = from;
            }
            auto *before_incision = after_incision->prev;
            after_incision->prev = to->prev;
//            to->prev != nullptr because to != donor.first
            to->prev->next = after_incision;
            to->prev = from->prev;
            from->prev = before_incision;
        }
    };
}
#pragma once

#include <iterator>

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
                next = nullptr;
            }
            if (next != nullptr) {
                next->prev = prev;
                prev = nullptr;
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
            current = current->next;
            return *this;
        }

        list_iterator &operator--() & noexcept {
            current = current->prev;
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
            splice(begin(), other, other.begin(), other.end());
        }

        ~list() {}

        list &operator=(list const &) = delete;

        list &operator=(list &&other) noexcept {
            clear();
            splice(begin(), other, other.begin(), other.end());
            return *this;
        }

        void clear() noexcept {
            first = &after_last;
            after_last.prev = nullptr;
        }

        list_element<Tag> after_last{};
        list_element<Tag> *first = &after_last;

        /*
        Поскольку вставка изменяет данные в list_element
        мы принимаем неконстантный T&.
        */

        void push_back(T &elem) noexcept {
            insert(end(), elem);
        }

        void pop_back() noexcept {
            erase(const_iterator(after_last.prev));
        }

        T &back() noexcept {
            return static_cast<T &>(*after_last.prev);
        }

        T const &back() const noexcept {
            return static_cast<const T &>(*after_last.prev);
        }

        void push_front(T &elem) noexcept {
            insert(begin(), elem);
        }

        void pop_front() noexcept {
            erase(begin());
        }

        T &front() noexcept {
            return static_cast<T &>(*first);
        }

        T const &front() const noexcept {
            return static_cast<const T &>(*first);
        }

        [[nodiscard]] bool empty() const noexcept {
            return first == &after_last;
        }

        iterator begin() noexcept {
            return iterator(first);
        }

        const_iterator begin() const noexcept {
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
                    donor.first = &donor.after_last;
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
#ifndef ADS_SET_H
#define ADS_SET_H

#ifdef DEBUG
#define TRACEON(msg, on) (on) << msg << std::endl;
#define TRACE(msg) TRACEON(msg, std::cout)
#define TRACE_DEB(msg) TRACEON("DEBUG: " << msg, std::cout)
#define TRACE_INF(msg) TRACEON("INFO: " << msg, std::cout)
#define TRACE_WRN(msg) TRACEON("WARN: " << msg, std::cerr)
#define TRACE_ERR(msg) TRACEON("ERROR: " << msg, std::cerr)
#define TRACE_IF(cond, msg) if (cond) { TRACE(msg) }
#define TRACE_DEB_IF(cond, msg) if (cond) { TRACE_DEB(msg) }
#define TRACE_INF_IF(cond, msg) if (cond) { TRACE_INF(msg) }
#define TRACE_WRN_IF(cond, msg) if (cond) { TRACE_WRN(msg) }
#define TRACE_ERR_IF(cond, msg) if (cond) { TRACE_ERR(msg) }
#else
#define TRACEON(msg, on)
#define TRACE(msg)
#define TRACE_DEB(msg)
#define TRACE_INF(msg)
#define TRACE_WRN(msg)
#define TRACE_ERR(msg)
#define TRACE_IF(cond, msg)
#define TRACE_DEB_IF(cond, msg)
#define TRACE_INF_IF(cond, msg)
#define TRACE_WRN_IF(cond, msg)
#define TRACE_ERR_IF(cond, msg)
#endif

#include<algorithm>

inline int invert(int n) {
    return -(n + 1);
}

// v2, now with 100% less memory leaks!
template<typename Key, size_t N = 2>
class ADS_set {
public:
    class Iterator;

    using value_type = Key;
    using key_type = Key;
    using reference = key_type&;
    using const_reference = const key_type&;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;
    using iterator = Iterator;
    using const_iterator = Iterator;
    using key_compare = std::less<key_type>;
    using key_equal = std::equal_to<key_type>;

private:
    enum class NodeType {
        INTERNAL,
        EXTERNAL
    };
    enum class InsertState {
        SUCCESS,
        EXISTS,
        TRIGGER_SPLIT
    };
    enum class EraseState {
        SUCCESS,
        NOT_FOUND,
        TRIGGER_MERGE
    };
    struct Node;
    struct InternalNode;
    struct ExternalNode;
    using link = Node*;

    static constexpr std::equal_to<key_type> eq = key_equal{};
    link root;
    size_type sz{};

public:
    ADS_set() : root{ new ExternalNode() }, sz{ 0 } {
        TRACE_DEB("ADS_set constructed via default constructor")
    }

    ADS_set(std::initializer_list<key_type> ilist) : root{ new ExternalNode() }, sz{ 0 } {
        for (const key_type& elem: ilist) {
            insert(elem);
        }
        TRACE_DEB("ADS_set constructed via i-list constructor")
    }

    template<typename InputIt>
    ADS_set(InputIt first, InputIt last) : root{ new ExternalNode() }, sz{ 0 } {
        for (InputIt it{ first }; it != last; ++it) {
            insert(*it);
        }
        TRACE_DEB("ADS_set constructed via range constructor")
    }

    ADS_set(const ADS_set& other) : ADS_set(other.begin(), other.end()) {}

    ~ADS_set() {
        TRACE_DEB("Deconstructing ADS_set")
        delete root;
        sz = 0;
    }

    ADS_set& operator=(const ADS_set& other) {
        clear();
        insert(other.begin(), other.end());
        return *this;
    }

    ADS_set& operator=(std::initializer_list<key_type> ilist) {
        clear();
        insert(ilist.begin(), ilist.end());
        return *this;
    }

    [[nodiscard]] size_type size() const {
        TRACE_DEB("Returning set size " << sz)
        return sz;
    }

    [[nodiscard]] bool empty() const {
        TRACE_DEB("ADS_set is" << (sz != 0 ? " not " : "") << "empty")
        return sz == 0;
    }

    void insert(std::initializer_list<key_type> ilist) {
        for (const key_type& elem: ilist) {
            insert(elem);
        }
    }

    std::pair<iterator, bool> insert(const key_type& key) {
        TRACE_INF("Inserting element: " << key)
        TRACE_DEB("Size (prev): " << sz)

        std::pair<iterator, InsertState> result{ root->add_elem(key) };
        switch (result.second) {
            case InsertState::SUCCESS: // size + 1, return true
                TRACE_DEB("Insert successful at top level")
                ++sz;
                return std::pair<iterator, bool>(result.first, true);
            case InsertState::EXISTS: // size + 0, return false
                TRACE_DEB("Insert ignored, element exists already")
                return std::pair<iterator, bool>(result.first, false);
            case InsertState::TRIGGER_SPLIT: // split, size + 1, return true
                TRACE_DEB("Insert triggered root split")
                std::pair<link, const key_type*> splitres{ root->split() };
                const key_type& new_index{ splitres.second ? *splitres.second : splitres.first->values[0] };
                root = new InternalNode(new_index, root, splitres.first);
                ++sz;
                return std::pair(root->find(key), true);
        }
        TRACE_ERR("Should never reach this code path")
        return {}; // unreachable
    }

    template<typename InputIt>
    void insert(InputIt first, InputIt last) {
        for (InputIt it{ first }; it != last; ++it) {
            insert(*it);
        }
    }

    void clear() {
        TRACE_DEB("Clearing ADS_set")
        delete root;
        root = new ExternalNode();
        sz = 0;
    }

    size_type erase(const key_type& key) {
        TRACE_INF("Erasing element: " << key)
        TRACE_DEB("Size (prev): " << sz)
        EraseState result{ root->remove_elem(key) };

        switch (result) {
            case EraseState::SUCCESS: // size - 1, return 1
                TRACE_DEB("Erase successful at top level")
                --sz;
                return 1;
            case EraseState::NOT_FOUND: // size - 0, return 0
                TRACE_DEB("Erase ignored, element does not exist")
                return 0;
            case EraseState::TRIGGER_MERGE: // ignore signal if nodesize > 0, size - 1, return 1
                if (root->size == 0) {
                    TRACE_DEB("Erase triggered root merge")
                    if (root->type() == NodeType::INTERNAL) {
                        std::pair<key_type*, link*> root_elems = root->get_all();
                        delete root;
                        root = root_elems.second[0];
                        delete[] root_elems.first;
                        delete[] root_elems.second;
                    } else {
                        delete root; // tree is now empty
                        root = new ExternalNode();
                    }
                }
                --sz;
                return 1;
        }
        TRACE_ERR("Should never reach this code path")
        return 0; // unreachable
    }

    size_type count(const key_type& key) const {
        TRACE_DEB("Counting element '" << key << '\'')
        return root->find(key) != end() ? 1 : 0;
    }

    iterator find(const key_type& key) const {
        TRACE_DEB("Searching element '" << key << '\'')
        return root->find(key);
    }

    void swap(ADS_set& other) {
        std::swap(sz, other.sz);
        std::swap(root, other.root);
    }

    const_iterator begin() const {
        return root->begin_it();
    }

    const_iterator end() const {
        return Iterator();
    }

    void dump(std::ostream& o = std::cerr) const {
        o << "B+ TREE: ADS_set<" << typeid(key_type).name() << ", " << N << ">, size: " << sz << std::endl;
        o << "Sorted elements:";
        for (iterator it{ begin() }; it != end(); ++it) {
            o << ' ' << *it;
        }
        o << std::endl << "Structure:" << std::endl;
        root->dump(o, 0);
        o << std::endl;
    }

    bool operator==(const ADS_set& rhs) const {
        if (sz != rhs.sz) return false;
        for (const_iterator itl{ begin() }, itr{ rhs.begin() }; itl != end(); ++itl, ++itr) {
            if (!eq(*itl, *itr)) return false;
        }
        return true;
    }

    bool operator!=(const ADS_set& rhs) const {
        return !operator==(rhs);
    }
};

template<typename Key, size_t N>
class ADS_set<Key, N>::Iterator {
public:
    using value_type = Key;
    using difference_type = std::ptrdiff_t;
    using reference = const value_type&;
    using pointer = const value_type*;
    using iterator_category = std::forward_iterator_tag;

private:
    ExternalNode* current;
    size_type pos;

public:
    Iterator() : current{ nullptr }, pos{ 0 } {}

    explicit Iterator(ExternalNode* _current, size_type _pos) : current{ _current }, pos{ _pos } {}

    reference operator*() const {
        return current->values[pos];
    }

    pointer operator->() const {
        return current->values + pos;
    }

    Iterator& operator++() {
        if (current) {
            if (pos + 1 == current->size) {
                current = current->next;
                pos = 0;
            } else {
                ++pos;
            }
        }
        return *this;
    }

    Iterator operator++(int) {
        Iterator old{ *this };
        this->operator++();
        return old;
    }

    bool operator==(const Iterator& rhs) const {
        return current == rhs.current && pos == rhs.pos;
    }

    bool operator!=(const Iterator& rhs) const {
        return current != rhs.current || pos != rhs.pos;
    }
};

template<typename Key, size_t N>
struct ADS_set<Key, N>::Node {
    static constexpr size_type M{ N * 2 }; // max size
    static constexpr std::less<key_type> cmp = key_compare{};
    key_type* values;
    size_type size;

    // returns i if found at position i, or -(i + 1) if insertion should happen at i
    int findpos(const key_type& elem) {
        if (size == 0 || cmp(elem, values[0])) return -1;
        for (size_type i{ 1 }; i < size; ++i) {
            if (cmp(elem, values[i])) return cmp(values[i - 1], elem) ? invert(static_cast<int>(i)) : i - 1; // + / 0 for found, - for insertion point
        }
        return !cmp(values[size - 1], elem) ? size - 1 : invert(size);
    }

    inline size_type findpos_autoinvert(const key_type& elem) {
        int pos{ findpos(elem) };
        return pos < 0 ? invert(pos) : pos;
    }

    // not safe if size >= N + 1
    virtual void insert_at(const key_type& elem, size_type ins) {
        for (size_type i{ size }; i > ins; --i) {
            values[i] = values[i - 1];
        }
        values[ins] = elem;
        ++size;
    }

    virtual void erase_at(size_type at) {
        for (size_type i{ at }; i < size - 1; ++i) {
            values[i] = values[i + 1];
        }
        --size;
    }

    // temporary invalid nodes require + 1
    explicit Node(size_type _size = 0) : values{ new key_type[M + 1] }, size{ _size } {}

    Node(const key_type* _values, size_type _size) : values{ new key_type[M + 1] }, size{ _size } {
        for (size_type i{ 0 }; i < _size; ++i) {
            values[i] = _values[i];
        }
    }

    virtual NodeType type() = 0;

    virtual iterator begin_it() = 0;

    virtual iterator find(const key_type& elem) = 0;

    virtual std::pair<iterator, InsertState> add_elem(const key_type& elem) = 0;

    virtual EraseState remove_elem(const key_type& elem) = 0;

    virtual std::pair<link, const key_type*> split(size_type split_at) = 0;

    std::pair<link, const key_type*> split() {
        return split((size - 1) / 2); // size to index conversion
    }

    virtual std::pair<key_type*, link*> get_all() = 0;

    virtual void prepare_merge(const key_type& pulled_down) = 0;

    virtual void merge(link neighbour) = 0;

    virtual void dump(std::ostream& o, size_type level) {
        if (level == 0) {
            o << "[ROOT]";
        } else {
            o << '[' << level << ']';
        }
        o << " [";
        switch (type()) {
            case NodeType::INTERNAL:
                o << "INTERNAL";
                break;
            case NodeType::EXTERNAL:
                o << "EXTERNAL";
                break;
        }
        o << " <" << this->size << "/" << Node::M << "> (" << (this->size * 100.0) / Node::M << "%)]";
        for (size_type i{ 0 }; i < this->size; ++i) {
            o << " (" << i << ")" << this->values[i];
        }
    }

    virtual ~Node() {
        delete[] values;
    }
};

template<typename Key, size_t N>
struct ADS_set<Key, N>::InternalNode : public Node {
    link* children;
    bool ownership;

    // temporary invalid nodes require + 2
    InternalNode() : Node(), children{ new link[Node::M + 2] }, ownership{ true } {
        children[0] = new ExternalNode();
    }

    InternalNode(const key_type* _values, const link* _children, size_type _size)
            : Node(_values, _size),
              children{ new link[Node::M + 2] },
              ownership{ true } {
        for (size_type i{ 0 }; i <= _size; ++i) {
            children[i] = _children[i];
        }
    }

    InternalNode(const key_type& value, link left, link right) : Node(1), children{ new link[Node::M + 2] }, ownership{ true } {
        this->values[0] = value;
        children[0] = left;
        children[1] = right;
    }

    inline size_type find_child_pos(const key_type& elem) {
        int pos{ this->findpos(elem) };
        return static_cast<size_type>(pos < -1 ? invert(pos) : (pos + 1));
    }

    void insert_at(const key_type& elem, size_type ins) override {
        for (size_type i{ this->size }; i > ins; --i) {
            this->values[i] = this->values[i - 1];
            this->children[i + 1] = this->children[i];
        }
        this->values[ins] = elem;
        ++this->size;
    }

    void erase_at(size_type at) override {
        delete children[at + 1]; // ownership should have been transferred
        for (size_type i{ at }; i < this->size - 1; ++i) {
            this->values[i] = this->values[i + 1];
            this->children[i + 1] = this->children[i + 2];
        }
        --this->size;
    }

    NodeType type() override {
        return NodeType::INTERNAL;
    }

    iterator begin_it() override {
        return children[0]->begin_it();
    }

    iterator find(const key_type& elem) override {
        size_type childpos{ find_child_pos(elem) };
        return children[childpos]->find(elem);
    }

    std::pair<iterator, InsertState> add_elem(const key_type& elem) override {
        size_type childpos{ find_child_pos(elem) };

        std::pair<iterator, InsertState> result{ children[childpos]->add_elem(elem) };
        if (result.second == InsertState::TRIGGER_SPLIT) { // split has to happen
            std::pair<link, const key_type*> splitres{ children[childpos]->split() };
            const key_type& new_index{ splitres.second ? *splitres.second : splitres.first->values[0] };
            size_type new_pos{ this->findpos_autoinvert(new_index) };

            this->insert_at(new_index, new_pos);
            children[new_pos + 1] = splitres.first;

            return std::pair<iterator, InsertState>(
                    find(elem),
                    this->size <= Node::M ? InsertState::SUCCESS : InsertState::TRIGGER_SPLIT // trigger split in parent if temporarily invalid
            );
        }
        return result; // was successfully added to child node (or already existed)
    }

    EraseState remove_elem(const key_type& elem) override {
        size_type childpos{ find_child_pos(elem) };

        EraseState result{ children[childpos]->remove_elem(elem) };
        if (result == EraseState::TRIGGER_MERGE) {
            if constexpr(N > 1) {
                childpos = childpos < 1 ? 1 : childpos;
                key_type& id = this->values[childpos - 1];
                link left{ children[childpos - 1] };
                link right{ children[childpos] };
                size_type totalsize{ left->size + right->size + (right->type() == NodeType::INTERNAL ? 1 : 0) };
                if (totalsize > Node::M) { // split (rebalance) if greater than M (including pulled-down index key -> + 1)
                    size_type split_at{ (totalsize - 1) / 2 }; // size to index conversion
                    std::pair<link, const key_type*> splitres;
                    if (split_at < left->size) { // split left, merge right into split result, split result is new right
                        splitres = left->split(split_at);
                        splitres.first->prepare_merge(id);
                        splitres.first->merge(right);
                    } else { // split right, merge right into left, split result is new right
                        splitres = right->split(split_at - left->size);
                        left->prepare_merge(id);
                        left->merge(right);
                    }
                    key_type new_index{ splitres.second ? *splitres.second : splitres.first->values[0] };
                    delete children[childpos]; // ownership should have been transferred
                    children[childpos] = splitres.first;
                    id = new_index;
                } else { // transport all elements from right to left
                    left->prepare_merge(id);
                    left->merge(right); // pull from right, append to left

                    this->erase_at(childpos - 1);
                }
            } else {
                if (childpos == 0) {
                    ++childpos;
                }

                children[childpos - 1]->prepare_merge(this->values[childpos - 1]);
                children[childpos - 1]->merge(children[childpos]);
                if (children[childpos - 1]->size > Node::M) { // internal node + two on the left
                    std::pair<link, const key_type*> splitres{ children[childpos - 1]->split(1) };
                    delete children[childpos];
                    children[childpos] = splitres.first;
                    this->values[childpos - 1] = splitres.second ? *splitres.second : splitres.first->values[0];
                } else { // external node or internal node with one on the left
                    this->erase_at(childpos - 1);
                }
            }
            return this->size >= N ? EraseState::SUCCESS : EraseState::TRIGGER_MERGE; // trigger merge in parent if temporarily invalid
        }

        return result; // was successfully deleted from child node (or not found)
    }

    std::pair<link, const key_type*> split(size_type split_at) override {
        // construct arrays for right node
        key_type right[Node::M + 1];
        link right_children[Node::M + 2];
        size_type right_size{ this->size - split_at - 1 };
        for (size_type i{ 0 }; i < right_size; ++i) {
            right[i] = this->values[split_at + 1 + i];
            right_children[i] = children[split_at + 1 + i];
        }
        right_children[right_size] = children[this->size];

        // cut array for left node (this)
        this->size = split_at;

        // construct pair to return
        return std::pair<link, key_type*>(new InternalNode(right, right_children, right_size), this->values + split_at);
    }

    std::pair<key_type*, link*> get_all() override {
        key_type* vals = new key_type[this->size];
        link* links = new link[this->size + 1];
        for (size_type i{ 0 }; i < this->size; ++i) {
            vals[i] = this->values[i];
            links[i] = children[i];
        }
        links[this->size] = children[this->size];
        ownership = false; // values are now owned by calling node

        return std::pair<key_type*, link*>(vals, links);
    }

    void prepare_merge(const key_type& pulled_down) override {
        this->values[this->size] = pulled_down;
        ++this->size;
    }

    void merge(link neighbour) override {
        std::pair<key_type*, link*> elems{ neighbour->get_all() };
        size_type prev_size{ this->size };
        for (size_type i{ 0 }; i < neighbour->size; ++i) {
            children[i + prev_size] = elems.second[i];
            this->values[i + prev_size] = elems.first[i];
        }
        this->size += neighbour->size;
        children[this->size] = elems.second[neighbour->size];

        delete[] elems.first;
        delete[] elems.second;
    }

    void dump(std::ostream& o, size_type level) override {
        Node::dump(o, level);
        for (size_type i{ 0 }; i <= this->size; ++i) {
            o << "\n\t" << i << ". ";
            children[i]->dump(o, level + 1);
        }
    }

    ~InternalNode() override {
        if (ownership) {
            for (size_type i{ 0 }; i <= this->size; ++i) {
                delete children[i];
            }
        }
        delete[] children;
    }
};

template<typename Key, size_t N>
struct ADS_set<Key, N>::ExternalNode : public Node {
    ExternalNode* next;

    ExternalNode() : Node(), next{ nullptr } {}

    ExternalNode(const key_type* _values, size_type _size, ExternalNode* _next = nullptr) : Node(_values, _size), next{ _next } {}

    NodeType type() override {
        return NodeType::EXTERNAL;
    }

    iterator begin_it() override {
        if (this->size == 0) return Iterator(); // end iterator
        return Iterator(this, 0);
    }

    iterator find(const key_type& elem) override {
        int pos{ this->findpos(elem) };
        if (pos < 0) return Iterator(); // not found, end iterator returned
        return Iterator(this, pos);
    }

    std::pair<iterator, InsertState> add_elem(const key_type& elem) override {
        int pos{ this->findpos(elem) };
        if (pos >= 0) return std::pair<iterator, InsertState>(Iterator(this, pos), InsertState::EXISTS); // ignore existing
        size_type inv_pos{ static_cast<size_type>(invert(pos)) };
        this->insert_at(elem, inv_pos);

        return std::pair<iterator, InsertState>(
                Iterator(this, inv_pos),
                this->size <= Node::M ? InsertState::SUCCESS : InsertState::TRIGGER_SPLIT // trigger split in parent if temporarily invalid
        );
    }

    EraseState remove_elem(const key_type& elem) override {
        int pos{ this->findpos(elem) };
        if (pos < 0) return EraseState::NOT_FOUND; // signal not found
        this->erase_at(static_cast<size_type>(pos));

        return this->size >= N ? EraseState::SUCCESS : EraseState::TRIGGER_MERGE; // trigger merge in parent if temporarily invalid
    }

    std::pair<link, const key_type*> split(size_type split_at) override {
        // construct array for right node
        key_type right_arr[Node::M + 1];
        size_type right_size{ this->size - split_at - 1 };
        for (size_type i{ 0 }; i < right_size; ++i) {
            right_arr[i] = this->values[split_at + 1 + i];
        }

        // cut array for left array (this)
        this->size = split_at + 1;

        // construct pair to return
        ExternalNode* right{ new ExternalNode(right_arr, right_size, next) };
        next = right;
        return std::pair<link, key_type*>(right, nullptr);
    }

    std::pair<key_type*, link*> get_all() override {
        key_type* vals = new key_type[this->size];
        for (size_type i{ 0 }; i < this->size; ++i) {
            vals[i] = this->values[i];
        }
        return std::pair<key_type*, link*>(vals, nullptr);
    }

    void prepare_merge(const key_type&) override {
        // no-op
    }

    void merge(link neighbour) override {
        std::pair<key_type*, link*> elems{ neighbour->get_all() };
        size_type prev_size{ this->size };

        // ignore elems.second, should be empty anyways
        for (size_type i{ 0 }; i < neighbour->size; ++i) {
            this->values[i + prev_size] = elems.first[i];
        }
        this->size += neighbour->size;
        delete[] elems.first;

        TRACE_INF_IF(next != neighbour, "Merge without pointer advance (if not in rebalance, this is a problem)")
        if (next == neighbour) {
            next = next->next;
        }
    }
};

template<typename Key, size_t N>
void swap(ADS_set<Key, N>& lhs, ADS_set<Key, N>& rhs) {
    lhs.swap(rhs);
}

#endif

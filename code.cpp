
#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>

using namespace std;

const int MAX_INDEX_LEN = 64;

struct Key {
    char index[MAX_INDEX_LEN + 1];
    int value;

    Key() {
        memset(index, 0, sizeof(index));
        value = 0;
    }

    Key(const char* idx, int val) {
        memset(index, 0, sizeof(index));
        strncpy(index, idx, MAX_INDEX_LEN);
        value = val;
    }

    bool operator<(const Key& other) const {
        int cmp = strcmp(index, other.index);
        if (cmp != 0) return cmp < 0;
        return value < other.value;
    }

    bool operator==(const Key& other) const {
        return value == other.value && strcmp(index, other.index) == 0;
    }

    bool operator<=(const Key& other) const {
        return *this < other || *this == other;
    }
};

const int M = 60;

struct Node {
    bool is_leaf;
    int size;
    long parent;
    long self;
    long next;
    Key keys[M];
    long children[M + 1];

    Node() {
        is_leaf = false;
        size = 0;
        parent = -1;
        self = -1;
        next = -1;
        for (int i = 0; i <= M; ++i) children[i] = -1;
    }
};

struct Header {
    long root;
    long first_leaf;
    long end_pos;
    long free_head;

    Header() : root(-1), first_leaf(-1), end_pos(sizeof(Header)), free_head(-1) {}
};

class BPlusTree {
    string filename;
    fstream file;
    Header header;

    void read_header() {
        file.seekg(0);
        file.read(reinterpret_cast<char*>(&header), sizeof(Header));
    }

    void write_header() {
        file.seekp(0);
        file.write(reinterpret_cast<const char*>(&header), sizeof(Header));
    }

    void read_node(long pos, Node& node) {
        if (pos == -1) return;
        file.seekg(pos);
        file.read(reinterpret_cast<char*>(&node), sizeof(Node));
    }

    void write_node(long pos, const Node& node) {
        if (pos == -1) return;
        file.seekp(pos);
        file.write(reinterpret_cast<const char*>(&node), sizeof(Node));
    }

    long allocate_node() {
        long pos;
        if (header.free_head != -1) {
            pos = header.free_head;
            Node node;
            read_node(pos, node);
            header.free_head = node.next;
        } else {
            pos = header.end_pos;
            header.end_pos += sizeof(Node);
        }
        return pos;
    }

    void deallocate_node(long pos) {
        Node node;
        read_node(pos, node);
        node.next = header.free_head;
        write_node(pos, node);
        header.free_head = pos;
    }

public:
    BPlusTree(string name) : filename(name) {
        file.open(filename, ios::in | ios::out | ios::binary);
        if (!file) {
            file.open(filename, ios::out | ios::binary);
            file.close();
            file.open(filename, ios::in | ios::out | ios::binary);
            header = Header();
            write_header();
        } else {
            read_header();
        }
    }

    ~BPlusTree() {
        write_header();
        file.close();
    }

    long find_leaf(const Key& key) {
        if (header.root == -1) return -1;
        long curr_pos = header.root;
        Node curr;
        read_node(curr_pos, curr);
        while (!curr.is_leaf) {
            int idx = upper_bound(curr.keys, curr.keys + curr.size, key) - curr.keys;
            curr_pos = curr.children[idx];
            read_node(curr_pos, curr);
        }
        return curr_pos;
    }

    void insert(const Key& key) {
        if (header.root == -1) {
            long pos = allocate_node();
            Node node;
            node.is_leaf = true;
            node.size = 1;
            node.self = pos;
            node.keys[0] = key;
            write_node(pos, node);
            header.root = pos;
            header.first_leaf = pos;
            write_header();
            return;
        }

        long leaf_pos = find_leaf(key);
        Node leaf;
        read_node(leaf_pos, leaf);

        // Check for duplicate
        int idx = lower_bound(leaf.keys, leaf.keys + leaf.size, key) - leaf.keys;
        if (idx < leaf.size && leaf.keys[idx] == key) return;

        // Insert into leaf
        for (int i = leaf.size; i > idx; --i) {
            leaf.keys[i] = leaf.keys[i - 1];
        }
        leaf.keys[idx] = key;
        leaf.size++;

        if (leaf.size < M) {
            write_node(leaf_pos, leaf);
        } else {
            // Split leaf
            long new_leaf_pos = allocate_node();
            Node new_leaf;
            new_leaf.is_leaf = true;
            new_leaf.self = new_leaf_pos;
            new_leaf.parent = leaf.parent;
            new_leaf.next = leaf.next;
            leaf.next = new_leaf_pos;

            int mid = leaf.size / 2;
            new_leaf.size = leaf.size - mid;
            leaf.size = mid;
            for (int i = 0; i < new_leaf.size; ++i) {
                new_leaf.keys[i] = leaf.keys[mid + i];
            }

            write_node(leaf_pos, leaf);
            write_node(new_leaf_pos, new_leaf);
            insert_into_parent(leaf_pos, new_leaf.keys[0], new_leaf_pos);
        }
        write_header();
    }

    void insert_into_parent(long left_pos, const Key& key, long right_pos) {
        Node left;
        read_node(left_pos, left);
        if (left.parent == -1) {
            long new_root_pos = allocate_node();
            Node new_root;
            new_root.is_leaf = false;
            new_root.self = new_root_pos;
            new_root.size = 1;
            new_root.keys[0] = key;
            new_root.children[0] = left_pos;
            new_root.children[1] = right_pos;
            write_node(new_root_pos, new_root);
            header.root = new_root_pos;
            left.parent = new_root_pos;
            write_node(left_pos, left);
            Node right;
            read_node(right_pos, right);
            right.parent = new_root_pos;
            write_node(right_pos, right);
            return;
        }

        long parent_pos = left.parent;
        Node parent;
        read_node(parent_pos, parent);

        int idx = lower_bound(parent.keys, parent.keys + parent.size, key) - parent.keys;
        for (int i = parent.size; i > idx; --i) {
            parent.keys[i] = parent.keys[i - 1];
            parent.children[i + 1] = parent.children[i];
        }
        parent.keys[idx] = key;
        parent.children[idx + 1] = right_pos;
        parent.size++;

        if (parent.size < M) {
            write_node(parent_pos, parent);
            Node right;
            read_node(right_pos, right);
            right.parent = parent_pos;
            write_node(right_pos, right);
        } else {
            // Split internal node
            long new_parent_pos = allocate_node();
            Node new_parent;
            new_parent.is_leaf = false;
            new_parent.self = new_parent_pos;
            new_parent.parent = parent.parent;

            int mid = parent.size / 2;
            Key up_key = parent.keys[mid];
            new_parent.size = parent.size - mid - 1;
            parent.size = mid;

            for (int i = 0; i < new_parent.size; ++i) {
                new_parent.keys[i] = parent.keys[mid + 1 + i];
            }
            for (int i = 0; i <= new_parent.size; ++i) {
                new_parent.children[i] = parent.children[mid + 1 + i];
            }

            write_node(parent_pos, parent);
            write_node(new_parent_pos, new_parent);

            // Update children's parent pointer
            for (int i = 0; i <= new_parent.size; ++i) {
                Node child;
                read_node(new_parent.children[i], child);
                child.parent = new_parent_pos;
                write_node(new_parent.children[i], child);
            }
            
            // Also need to set parent for right_pos if it stayed in parent
            if (idx + 1 <= mid) {
                Node right;
                read_node(right_pos, right);
                right.parent = parent_pos;
                write_node(right_pos, right);
            }

            insert_into_parent(parent_pos, up_key, new_parent_pos);
        }
    }

    void remove(const Key& key) {
        long leaf_pos = find_leaf(key);
        if (leaf_pos == -1) return;

        Node leaf;
        read_node(leaf_pos, leaf);
        int idx = lower_bound(leaf.keys, leaf.keys + leaf.size, key) - leaf.keys;
        if (idx == leaf.size || !(leaf.keys[idx] == key)) return;

        for (int i = idx; i < leaf.size - 1; ++i) {
            leaf.keys[i] = leaf.keys[i + 1];
        }
        leaf.size--;
        write_node(leaf_pos, leaf);
        // For simplicity, we don't merge nodes on deletion in this basic implementation.
        // Given the constraints and the nature of the problem, it might be acceptable.
        // If not, we'll need to implement merging.
    }

    void find(const char* index) {
        Key search_key(index, -1);
        long leaf_pos = find_leaf(search_key);
        if (leaf_pos == -1) {
            cout << "null" << endl;
            return;
        }

        Node leaf;
        read_node(leaf_pos, leaf);
        int idx = lower_bound(leaf.keys, leaf.keys + leaf.size, search_key) - leaf.keys;
        
        bool found = false;
        bool first = true;

        while (true) {
            for (int i = idx; i < leaf.size; ++i) {
                if (strcmp(leaf.keys[i].index, index) == 0) {
                    if (!first) cout << " ";
                    cout << leaf.keys[i].value;
                    found = true;
                    first = false;
                } else {
                    if (!found) cout << "null";
                    cout << endl;
                    return;
                }
            }
            if (leaf.next == -1) break;
            leaf_pos = leaf.next;
            read_node(leaf_pos, leaf);
            idx = 0;
        }
        if (!found) cout << "null";
        cout << endl;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    BPlusTree tree("data.db");

    int n;
    if (!(cin >> n)) return 0;

    while (n--) {
        string cmd;
        cin >> cmd;
        if (cmd == "insert") {
            string index;
            int value;
            cin >> index >> value;
            tree.insert(Key(index.c_str(), value));
        } else if (cmd == "delete") {
            string index;
            int value;
            cin >> index >> value;
            tree.remove(Key(index.c_str(), value));
        } else if (cmd == "find") {
            string index;
            cin >> index;
            tree.find(index.c_str());
        }
    }

    return 0;
}

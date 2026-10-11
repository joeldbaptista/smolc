/*
 * tree.c -- a binary search tree of ints.
 *
 * A node is a struct node from malloc(), with a key and two
 * subtrees.  An empty subtree is NULL.
 *
 * As in list.c, the heap works like a stack, so deleted nodes go to a
 * free list and newnode() reuses them.
 */

#include stdio.h

#define QMAX 64 /* nodes the level-order queue holds */

struct node {
	int key;
	struct node *left, *right;
};

struct node *freelist; /* deleted nodes, chained through left */
int nalloc;            /* nodes taken from the heap */

/* Return a leaf holding k. */
struct node *
newnode(int k)
{
	struct node *n;

	if (freelist) {
		n = freelist;
		freelist = n->left;
	} else {
		if ((n = malloc(sizeof(struct node))) == NULL) {
			fputs("out of memory\n", stderr);
			exit(1);
		}
		++nalloc;
	}
	n->key = k;
	n->left = NULL;
	n->right = NULL;
	return n;
}

/* Put node n on the free list. */
void
release(struct node *n)
{
	n->left = freelist;
	freelist = n;
}

/* Insert k, unless it is already there.  Returns the root. */
struct node *
insert(struct node *t, int k)
{
	if (t == NULL)
		return newnode(k);
	if (k < t->key)
		t->left = insert(t->left, k);
	else if (k > t->key)
		t->right = insert(t->right, k);
	return t;
}

/* Return the node holding k, else NULL.  This one walks, not recurses. */
struct node *
lookup(struct node *t, int k)
{
	while (t) {
		if (k == t->key)
			return t;
		if (k < t->key)
			t = t->left;
		else
			t = t->right;
	}
	return NULL;
}

/* Return the node with the smallest key in the non-empty tree t. */
struct node *
leftmost(struct node *t)
{
	while (t->left)
		t = t->left;
	return t;
}

/*
 * Delete k, if it is there.  Returns the root.  A node with two
 * children takes the key of its in-order successor, the smallest key
 * on its right, and that successor is deleted instead.
 */
struct node *
delete(struct node *t, int k)
{
	struct node *child;

	if (t == NULL)
		return NULL;
	if (k < t->key) {
		t->left = delete(t->left, k);
		return t;
	}
	if (k > t->key) {
		t->right = delete(t->right, k);
		return t;
	}
	if (t->left && t->right) {
		t->key = leftmost(t->right)->key;
		t->right = delete(t->right, t->key);
		return t;
	}
	if (t->left)
		child = t->left;
	else
		child = t->right;
	release(t);
	return child;
}

int
count(struct node *t)
{
	if (t == NULL)
		return 0;
	return count(t->left) + 1 + count(t->right);
}

int
height(struct node *t)
{
	int l, r;

	if (t == NULL)
		return 0;
	l = height(t->left);
	r = height(t->right);
	if (l > r)
		return l + 1;
	return r + 1;
}

/* Print the keys in order: left subtree, node, right subtree. */
void
inorder(struct node *t)
{
	if (t == NULL)
		return;
	inorder(t->left);
	printf(" %d", t->key);
	inorder(t->right);
}

/* Print the keys node first, which is the order that rebuilds t. */
void
preorder(struct node *t)
{
	if (t == NULL)
		return;
	printf(" %d", t->key);
	preorder(t->left);
	preorder(t->right);
}

/* Print the keys level by level, through a queue of node addresses. */
void
levels(struct node *t)
{
	int q[QMAX]; /* node addresses: Small-C has no pointer arrays */
	int head, tail;
	struct node *n;

	if (t == NULL)
		return;
	head = tail = 0;
	q[tail++] = t;
	while (head < tail) {
		n = q[head++];
		printf(" %d", n->key);
		if (n->left && tail < QMAX)
			q[tail++] = n->left;
		if (n->right && tail < QMAX)
			q[tail++] = n->right;
	}
}

/*
 * Draw t on its side: the root at the left margin, the right subtree
 * above it and the left subtree below, four columns per level.
 */
void
draw(struct node *t, int depth)
{
	int i;

	if (t == NULL)
		return;
	draw(t->right, depth + 1);
	for (i = 0; i < depth; ++i)
		printf("    ");
	printf("%d\n", t->key);
	draw(t->left, depth + 1);
}

/* Put every node on the free list.  Returns NULL, the empty tree. */
struct node *
clear(struct node *t)
{
	if (t == NULL)
		return NULL;
	clear(t->left);
	clear(t->right);
	release(t);
	return NULL;
}

void
report(char *label, struct node *t)
{
	printf("%s\n  in-order: ", label);
	inorder(t);
	printf("\n  pre-order:");
	preorder(t);
	printf("\n  level:    ");
	levels(t);
	printf("\n  count = %d, height = %d\n", count(t), height(t));
}

int
main(void)
{
	int keys[11];
	struct node *root;
	int i;

	keys[0] = 50;
	keys[1] = 30;
	keys[2] = 70;
	keys[3] = 20;
	keys[4] = 40;
	keys[5] = 60;
	keys[6] = 80;
	keys[7] = 35;
	keys[8] = 45;
	keys[9] = 65;
	keys[10] = 40; /* a duplicate, which insert() ignores */

	root = NULL;
	for (i = 0; i < 11; ++i)
		root = insert(root, keys[i]);
	report("built:", root);
	draw(root, 0);

	printf("lookup 45: %d, lookup 55: %d\n", lookup(root, 45) != NULL,
	       lookup(root, 55) != NULL);
	printf("min = %d\n", leftmost(root)->key);

	root = delete(root, 60); /* one child */
	root = delete(root, 30); /* two children: 35 takes its place */
	root = delete(root, 20); /* a leaf */
	root = delete(root, 50); /* the root: 65 takes its place */
	root = delete(root, 99); /* absent */
	report("after deleting 60, 30, 20 and 50:", root);
	draw(root, 0);

	root = clear(root);
	for (i = 1; i <= 7; ++i)
		root = insert(root, i * 10);
	report("sorted input makes a chain:", root);
	printf("nodes allocated = %d\n", nalloc);
	return 0;
}

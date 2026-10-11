/*
 * list.c -- a singly linked list of ints.
 *
 * A node is a struct node from malloc(), linked through its next
 * member.  The end of the list is NULL.
 *
 * The heap works like a stack: free() releases a block and every block
 * allocated after it.  So a removed node cannot be freed on its own.
 * It goes to a free list instead, and newnode() reuses it.
 */

#include stdio.h

struct node {
	int val;
	struct node *next;
};

struct node *freelist; /* removed nodes, ready for reuse */
int nalloc;             /* nodes taken from the heap */

/* Return a node holding v, followed by next. */
struct node *
newnode(int v, struct node *next)
{
	struct node *n;

	if (freelist) {
		n = freelist;
		freelist = n->next;
	} else {
		if ((n = malloc(sizeof(struct node))) == NULL) {
			fputs("out of memory\n", stderr);
			exit(1);
		}
		++nalloc;
	}
	n->val = v;
	n->next = next;
	return n;
}

/* Put node n on the free list. */
void
release(struct node *n)
{
	n->next = freelist;
	freelist = n;
}

/* Add v at the front.  Returns the new head. */
struct node *
push(struct node *head, int v)
{
	return newnode(v, head);
}

/* Add v at the end.  Returns the head. */
struct node *
append(struct node *head, int v)
{
	struct node *p;

	if (head == NULL)
		return newnode(v, NULL);
	p = head;
	while (p->next)
		p = p->next;
	p->next = newnode(v, NULL);
	return head;
}

/* Insert v before the first larger value.  Returns the head. */
struct node *
insort(struct node *head, int v)
{
	struct node *p;

	if (head == NULL || v < head->val)
		return newnode(v, head);
	p = head;
	while (p->next && p->next->val <= v)
		p = p->next;
	p->next = newnode(v, p->next);
	return head;
}

/* Return the first node holding v, else NULL. */
struct node *
find(struct node *head, int v)
{
	while (head) {
		if (head->val == v)
			return head;
		head = head->next;
	}
	return NULL;
}

/* Remove the first node holding v.  Returns the head. */
struct node *
drop(struct node *head, int v)
{
	struct node *p, *q;

	if (head == NULL)
		return NULL;
	if (head->val == v) {
		q = head->next;
		release(head);
		return q;
	}
	p = head;
	while (p->next) {
		q = p->next;
		if (q->val == v) {
			p->next = q->next;
			release(q);
			break;
		}
		p = q;
	}
	return head;
}

/* Reverse the list in place.  Returns the new head. */
struct node *
revlist(struct node *head)
{
	struct node *prev, *next;

	prev = NULL;
	while (head) {
		next = head->next;
		head->next = prev;
		prev = head;
		head = next;
	}
	return prev;
}

int
length(struct node *head)
{
	int n;

	n = 0;
	while (head) {
		++n;
		head = head->next;
	}
	return n;
}

int
sum(struct node *head)
{
	int s;

	s = 0;
	while (head) {
		s += head->val;
		head = head->next;
	}
	return s;
}

/* Put every node on the free list.  Returns NULL, the empty list. */
struct node *
clear(struct node *head)
{
	struct node *next;

	while (head) {
		next = head->next;
		release(head);
		head = next;
	}
	return NULL;
}

void
show(char *label, struct node *head)
{
	printf("%-9s [", label);
	while (head) {
		printf("%d", head->val);
		if ((head = head->next))
			printf(", ");
	}
	printf("]\n");
}

int
main(void)
{
	int data[6];
	struct node *list;
	int i;

	list = NULL;
	for (i = 1; i <= 5; ++i)
		list = append(list, i * i);
	show("append:", list);
	list = push(list, 0);
	show("push 0:", list);
	printf("length = %d, sum = %d\n", length(list), sum(list));

	list = revlist(list);
	show("reverse:", list);
	if (find(list, 9))
		printf("find 9: yes\n");
	if (find(list, 7) == NULL)
		printf("find 7: no\n");

	list = drop(list, 25); /* the head */
	list = drop(list, 9);  /* the middle */
	list = drop(list, 0);  /* the tail */
	list = drop(list, 7);  /* absent */
	show("drop:", list);

	list = clear(list);
	data[0] = 5;
	data[1] = 3;
	data[2] = 8;
	data[3] = 1;
	data[4] = 9;
	data[5] = 2;
	for (i = 0; i < 6; ++i)
		list = insort(list, data[i]);
	show("insort:", list);
	printf("nodes allocated = %d, reused = %d\n", nalloc,
	       6 + 6 - nalloc);
	return 0;
}

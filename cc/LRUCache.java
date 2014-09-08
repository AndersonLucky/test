public class LRUCache {
    private HashMap<Integer,DoubleLinkedListNode> map = new HashMap<Integer,DoubleLinkedListNode>();
    private DoubleLinkedListNode head;
    private DoubleLinkedListNode end;
    private int len;
    private int capacity;
   
    public LRUCache(int capacity) {
        this.capacity = capacity;
        len = 0;
    }
   
    public int get(int key) {
        if(map.containsKey(key)) {
            DoubleLinkedListNode current = map.get(key);
            removeNode(current);
            setHead(current);
            return current.val;
        } else {
            return -1;
        }
    }
   
    public void set(int key, int val) {
        if(map.containsKey(key)) {
            DoubleLinkedListNode oldNode = map.get(key);
            oldNode.val = val;
            removeNode(oldNode);
            setHead(oldNode);
        } else {
            DoubleLinkedListNode newNode= new DoubleLinkedListNode(key,val);
            if(len < capacity) {
                setHead(newNode);
                map.put(key,newNode);
                len++;
            } else {
                map.remove(end.key);
                end = end.pre;
                if(end!=null) end.next = null;
                setHead(newNode);
                map.put(key,newNode);
            }
        }
    }
   
    public void setHead(DoubleLinkedListNode node) {
        node.next = head;
        node.pre = null;
        if(head != null) head.pre = node;
        head = node;
        if(end == null) end = node;
    }
   
    public void removeNode(DoubleLinkedListNode Node) {
        DoubleLinkedListNode curr = Node;
        DoubleLinkedListNode prev = curr.pre;
        DoubleLinkedListNode post = curr.next;
       
        if(prev!=null) prev.next = post;
        else head = post;
       
        if(post!=null) post.pre = prev;
        else end = prev;
       
    }
   
}

class DoubleLinkedListNode {
    public int val;
    public int key;
    public DoubleLinkedListNode pre;
    public DoubleLinkedListNode next;
   
    DoubleLinkedListNode(int key, int val) {
        this.val = val;
        this.key = key;
    }
}

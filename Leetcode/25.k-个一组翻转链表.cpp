/*
 * @lc app=leetcode.cn id=25 lang=cpp
 *
 * [25] K 个一组翻转链表
 */

// @lc code=start
/**
 * Definition for singly-linked list.
 * struct ListNode {
 *     int val;
 *     ListNode *next;
 *     ListNode() : val(0), next(nullptr) {}
 *     ListNode(int x) : val(x), next(nullptr) {}
 *     ListNode(int x, ListNode *next) : val(x), next(next) {}
 * };
 */

class Solution {
public:
    ListNode* reverse(ListNode* prev, ListNode* end) {
        ListNode* cur = prev->next;
        while (cur != end) {
            ListNode* nxt = cur->next;
            cur->next = prev;
            prev = cur;
            cur = nxt;
        }
        return prev;
    }

    ListNode* reverseKGroup(ListNode* head, int k) {
        ListNode* res = new ListNode(-1);
        res->next = head;
        ListNode* prev = res;

        while (cur) {
            int cnt = 0;
            while (cur && cnt < k) {
                cur = cur->next;
                cnt ++;
            }

            if (!cur) return res->next;
            ListNode* end = cur->next;
            prev = reverse(prev, end);
        }

        return res->next;
    }
};
// @lc code=end


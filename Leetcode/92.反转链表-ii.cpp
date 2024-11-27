/*
 * @lc app=leetcode.cn id=92 lang=cpp
 *
 * [92] 反转链表 II
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
    ListNode* reverseBetween(ListNode* head, int left, int right) {
        ListNode* res = new ListNode(-1);
        res -> next = head;
        ListNode* prev = res;

        for (int i = 0 ; i < left - 1; i ++) {
            prev = prev->next;
        }

        if (prev == nullptr) return head;

        ListNode* cur = prev->next;

        ListNode* nxt;

        for (int i = 0; i < right - left; i ++) {
            nxt = cur->next;
            cur->next = nxt->next;
            nxt->next = prev->next;
            prev->next = nxt;
        }

        return res->next;
    }
};
// @lc code=end


#include "ege_head.h"
#include "ege_common.h"

#include "mouse.h"

namespace ege
{

/*private function*/
static EGEMSG _getmouse(_graph_setting* pg)
{
    EGEMSG msg = {0};

    while (pg->msgmouse_queue->pop(msg)) {
        return msg;
    }
    return msg;
}

/*private function*/
static EGEMSG peekmouse(_graph_setting* pg)
{
    EGEMSG msg = {0};

    if (pg->msgmouse_queue->empty()) {
        dealmessage(pg, NORMAL_UPDATE);
    }
    while (pg->msgmouse_queue->pop(msg)) {
        pg->msgmouse_queue->unpop();
        return msg;
    }
    return msg;
}

void flushmouse()
{
    struct _graph_setting* pg = &graph_setting;
    EGEMSG                 msg;
    if (pg->msgmouse_queue->empty()) {
        dealmessage(pg, NORMAL_UPDATE);
    }
    if (!pg->msgmouse_queue->empty()) {
        while (pg->msgmouse_queue->pop(msg)) {
            ;
        }
    }
    return;
}

int mousemsg()
{
    struct _graph_setting* pg = &graph_setting;
    if (pg->exit_window) {
        return 0;
    }
    EGEMSG msg;
    msg = peekmouse(pg);
    if (msg.hwnd) {
        return 1;
    }
    return 0;
}

mouse_msg getmouse()
{
    struct _graph_setting* pg   = &graph_setting;
    mouse_msg              mmsg = {0};
    if (pg->exit_window) {
        return mmsg;
    }

    {
        EGEMSG msg;
        do {
            msg = _getmouse(pg);
            if (msg.hwnd) {
                mmsg.flags |= ((msg.wParam & MK_CONTROL) != 0 ? mouse_flag_ctrl : 0);
                mmsg.flags |= ((msg.wParam & MK_SHIFT) != 0 ? mouse_flag_shift : 0);
                mmsg.x      = (short)((int)msg.lParam & 0xFFFF);
                mmsg.y      = (short)((unsigned)msg.lParam >> 16);
                mmsg.msg    = mouse_msg_move;

                switch (msg.message) {
                case WM_LBUTTONDOWN:
                    mmsg.msg    = mouse_msg_down;
                    mmsg.flags |= mouse_flag_left;
                    break;
                case WM_LBUTTONUP:
                    mmsg.msg    = mouse_msg_up;
                    mmsg.flags |= mouse_flag_left;
                    break;
                case WM_RBUTTONDOWN:
                    mmsg.msg    = mouse_msg_down;
                    mmsg.flags |= mouse_flag_right;
                    break;
                case WM_RBUTTONUP:
                    mmsg.msg    = mouse_msg_up;
                    mmsg.flags |= mouse_flag_right;
                    break;
                case WM_MBUTTONDOWN:
                    mmsg.msg    = mouse_msg_down;
                    mmsg.flags |= mouse_flag_mid;
                    break;
                case WM_MBUTTONUP:
                    mmsg.msg    = mouse_msg_up;
                    mmsg.flags |= mouse_flag_mid;
                    break;
                case WM_MOUSEWHEEL:
                    mmsg.msg   = mouse_msg_wheel;
                    mmsg.wheel = (short)((unsigned)msg.wParam >> 16);
                    break;
                case WM_XBUTTONDOWN:
                    mmsg.msg    = mouse_msg_down;

                    if ((msg.wParam >> 16) & 0x0001) {
                        mmsg.flags |= mouse_flag_x1;
                    } else if ((msg.wParam >> 16) & 0x0002) {
                        mmsg.flags |= mouse_flag_x2;
                    }

                    break;
                case WM_XBUTTONUP:
                    mmsg.msg    = mouse_msg_up;

                    if ((msg.wParam >> 16) & 0x0001) {
                        mmsg.flags |= mouse_flag_x1;
                    } else if ((msg.wParam >> 16) & 0x0002) {
                        mmsg.flags |= mouse_flag_x2;
                    }
                    break;

                default:break;
                }

                return mmsg;
            }
        } while (!pg->exit_window && !pg->exit_flag && waitdealmessage(pg));
    }
    return mmsg;
}

MOUSEMSG GetMouseMsg()
{
    struct _graph_setting* pg   = &graph_setting;
    MOUSEMSG               mmsg = {0};
    if (pg->exit_window) {
        return mmsg;
    }

    {
        EGEMSG msg;
        do {
            msg = _getmouse(pg);
            if (msg.hwnd) {
                mmsg.uMsg    = msg.message;
                mmsg.mkCtrl  = ((msg.wParam & MK_CONTROL) != 0);
                mmsg.mkShift = ((msg.wParam & MK_SHIFT) != 0);
                mmsg.x       = (short)((int)msg.lParam & 0xFFFF);
                mmsg.y       = (short)((unsigned)msg.lParam >> 16);
                if (msg.mousekey & 0x01) {
                    mmsg.mkLButton = 1;
                }

                if (msg.mousekey & 0x02) {
                    mmsg.mkRButton = 1;
                }

                if (msg.mousekey & 0x04) {
                    mmsg.mkMButton = 1;
                }

                if (msg.mousekey & 0x08) {
                    mmsg.mkXButton1 = 1;
                }

                if (msg.mousekey & 0x10) {
                    mmsg.mkXButton2 = 1;
                }

                if (msg.message == WM_MOUSEWHEEL) {
                    mmsg.wheel = (short)((unsigned)msg.wParam >> 16);
                }
                return mmsg;
            }
        } while (!pg->exit_window && waitdealmessage(pg));
    }
    return mmsg;
}

}

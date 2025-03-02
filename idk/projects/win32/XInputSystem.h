#pragma once

#include <app/GamepadSystem.h>

struct _XINPUT_STATE;

namespace idk::win
{

    class XInputSystem : public GamepadSystem
    {
    public:
        void Init();
        void Shutdown();
        void Update();

        bool GetButtonDown(char player, GamepadButton button) const override;
        bool GetButtonUp(char player, GamepadButton button) const override;
        bool GetButton(char player, GamepadButton button) const override;
        float GetAxis(char player, GamepadAxis axis) const override;
        void SetRumble(char player, float low_freq, float high_freq) override;
        char GetConnectedPlayers() const override;

    private:
        struct x_gamepad_state
        {
            unsigned short buttons;
            float          left_trigger;
            float          right_trigger;
            float          thumb_left_x;
            float          thumb_left_y;
            float          thumb_right_x;
            float          thumb_right_y;
        };

        using Buffer = x_gamepad_state[4];
        Buffer _buffers[2];
        char _curr_buffer_index = 0;
        char _connected_users = 0;
        float _new_connection_check_timer = 0;

        Buffer& _curr_buf() { return _buffers[_curr_buffer_index]; }
        const Buffer& _curr_buf() const { return _buffers[_curr_buffer_index]; }
        Buffer& _prev_buf() { return _buffers[!_curr_buffer_index]; }
        const Buffer& _prev_buf() const { return _buffers[!_curr_buffer_index]; }
        void _swap_bufs() { _curr_buffer_index = !_curr_buffer_index; }
        x_gamepad_state _process_state(const _XINPUT_STATE& state);
    };

}
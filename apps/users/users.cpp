#include <utils/Graphics.hpp>
#include <utils/StdLib.hpp>
#include <utils/Widgets.hpp>
#include <utils/Thread.hpp>
#include <utils/Function.hpp>
#include <libc.h>
#include <colors.h>

class UserEditor : public Window {

public:
    UserEditor(int width, int height) : Window(200, 200, "User Editor", 1) {
        this->width = width;
        this->height = height;

        /* Create widgets */
        widgets = new WidgetManager(); 
        Layout* main = new Layout(
            10, 10,
            180, 160,
            VERTICAL,
            LAYOUT_FLAG_NONE
        );

        Layout* bottom = new Layout(
            10, 170,
            180, 24,
            HORIZONTAL,
            LAYOUT_FLAG_BORDER
        );

        main->addWidget(new Label("Create a new user"), CENTER);
        main->addWidget(new Spacing(0, 8), LEFT);
        main->addWidget(new Label("Username:"), LEFT);
        main->addWidget(new Input(100, 14, "Username", "#username"), LEFT);
        main->addWidget(new Label("Password:"), LEFT);
        main->addWidget(new Input(100, 14, "Password", "#password"), LEFT);
        main->addWidget(new Spacing(0, 8), LEFT);
        main->addWidget(new Label("Permissions:"), CENTER);
        main->addWidget(new Checkbox(false, "Admin"), LEFT);
        main->addWidget(new Checkbox(true, "User"), LEFT);
        main->addWidget(new Checkbox(false, "Guest"), LEFT);

        bottom->addWidget(new Button(50, 14, "Cancel", Function([this]() {
            delete widgets;
            exit();
        })), RIGHT);

        bottom->addWidget(new Button(50, 14, "Create", Function([this]() {
            printf("Username: %s\n", ((Input*)widgets->getByTag("#username"))->getData()); 
            printf("Password: %s\n", ((Input*)widgets->getByTag("#password"))->getData());
        })), RIGHT);


        widgets->addLayout(main);
        widgets->addLayout(bottom);
    }

    int eventHandler(struct gfx_event* event) {
        switch (event->event)
        {
        case GFX_EVENT_RESOLUTION:
            /* update screensize */
            break;
        case GFX_EVENT_EXIT:
            delete widgets;
            return -1;
        case GFX_EVENT_KEYBOARD:
            widgets->Keyboard(event->data);
            /* keyboard event in e.data */
            break;
        case GFX_EVENT_MOUSE:
            widgets->Mouse(event->data, event->data2);
            break;
        }
        return 0;
    }

    void draw() {
        /* Clear screen */
        drawRect(0, 0, width, height, 30);
        /* Draw widgets */
        widgets->draw(this);
    }

    int test = 1337;

private:
    int width;
    int height;
    WidgetManager* widgets;
};

void editorEntry(void* arg) {
    UserEditor t(200, 200);

    struct gfx_event e;
    while (1){
        gfx_get_event(&e, GFX_EVENT_BLOCKING); /* alt: GFX_EVENT_NONBLOCKING */
        if(t.eventHandler(&e) == -1) break;
        t.draw();
    }
    return;
}


class Users : public Window {
public:
    Users(int width, int height) : Window(200, 200, "Users", 1) {
        this->width = width;
        this->height = height;

        /* Create widgets */
        widgets = new WidgetManager();
        LayoutHandle main = widgets->addLayout(new Layout(
            10, 10,
            180, 180,
            VERTICAL,
            0

        ));

        widgets->addWidget(main, LEFT, new Button(100, 14, "Button", Function([this]() {
            printf("Button pressed!\n");
        })));
        widgets->addWidget(main, RIGHT, new Button(100, 14, "Start Edit", Function([this]() {

            Thread* editor = new Thread(editorEntry, 0);
            editor->start(0);

        })));

        Input* input = new Input(100, 12, "Input");
        widgets->addWidget(main, CENTER, input);
    
        widgets->addWidget(main, LEFT, new Checkbox(true));
        widgets->addWidget(main, LEFT, new Label("Checkbox"));
        widgets->addWidget(main, LEFT, new Checkbox(false));
    }

    int eventHandler(struct gfx_event* event) {
        switch (event->event){
        case GFX_EVENT_RESOLUTION:
            /* update screensize */
            break;
        case GFX_EVENT_EXIT:
            delete widgets;
            exit();
            /* exit */
            return -1;
        case GFX_EVENT_KEYBOARD:
            widgets->Keyboard(event->data);
            /* keyboard event in e.data */
            break;
        case GFX_EVENT_MOUSE:
            widgets->Mouse(event->data, event->data2);
            break;
        }
        return 0;
    }

    void draw() {
        /* Clear screen */
        drawRect(0, 0, width, height, 30);
        /* Draw widgets */
        widgets->draw(this);
    }

private:
    int width;
    int height;
    WidgetManager* widgets;
};

class MyClass {
public:
    void memberFunction() {
        Function f = [this]() { this->anotherFunction(); };
        f(); // Invoke the lambda
    }

    void anotherFunction() {
        printf("Hello from another function!\n");
    }
};


int main()
{
    MyClass c;
    c.memberFunction();

    Users t(200, 200);
    t.draw();

    struct gfx_event e;
    while (1){
        gfx_get_event(&e, GFX_EVENT_BLOCKING); /* alt: GFX_EVENT_NONBLOCKING */
        if(t.eventHandler(&e) == -1) break;
        t.draw();
    }
    return 0;
}
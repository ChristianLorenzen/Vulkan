#pragma once

namespace Faye {
    class Pipeline {
        public:
            Pipeline() = default;
            ~Pipeline() = default;

            Pipeline(const Pipeline &) = delete;
            void operator=(const Pipeline &) = delete;
        private:
    };
}
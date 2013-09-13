

#ifndef AVHTTP_INTER_THREAD_STREAM_HPP
#define AVHTTP_INTER_THREAD_STREAM_HPP

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>

namespace avhttp {

/*
 * 用法同 管道, 但是优点是不经过内核. 支持同步和异步读取.
 *
 * 注意, 该对象是线程安全的, 但是可以禁用线程安全特性, 如果你的代码是单线程的话
 *
 * 多次调用 async_read/some 是未定义行为.
 *
 * TODO: 暂时先实现 异步, 同步的稍后实现.
 */

class interthread_stream : boost::noncopyable
{
public:
	explicit interthread_stream(boost::asio::io_service & io_service)
		: m_io_service(io_service)
	{

	}

	template <typename ConstBufferSequence, typename Handler>
	void async_write_some(const ConstBufferSequence& buffers, BOOST_ASIO_MOVE_ARG(Handler) handler)
	{
	 	if (m_state_read_closed)
		{
			return m_io_service.post(
				boost::asio::detail::bind_handler(
					handler,
					boost::asio::error::make_error_code(boost::asio::error::broken_pipe),
					0
				)
			);
		}

		boost::mutex::scoped_lock l(m_mutex);

		std::size_t bytes_writed;

		bytes_writed = boost::asio::buffer_copy(m_buffer.prepare(boost::asio::buffer_size(buffers)),
			boost::asio::buffer(buffers));

		m_buffer.commit(bytes_writed);

		if (m_current_read_handler)
		{
			std::size_t bytes_readed = boost::asio::buffer_copy(
				boost::asio::buffer(m_read_buffer), m_buffer.data());
			m_buffer.consume(bytes_readed);

			// 唤醒协程.

			m_io_service.post(
				boost::asio::detail::bind_handler(
					m_current_read_handler,
					boost::system::error_code(),
					bytes_readed
				)
			);

			m_current_read_handler = NULL;
		}

		// 决定是否睡眠.

		if ( m_buffer.size() >= 512)
		{
			m_current_write_handler = handler;
			m_writed_size = bytes_writed;
		}else
		{
			m_io_service.post(
				boost::asio::detail::bind_handler(
					handler,
					boost::system::error_code(),
					bytes_writed
				)
			);
		}
	}

	template <typename MutableBufferSequence, typename Handler>
	void async_read_some(const MutableBufferSequence& buffers, BOOST_ASIO_MOVE_ARG(Handler) handler)
	{
		boost::mutex::scoped_lock l(m_mutex);

		if (m_buffer.size() == 0)
		{
			if (m_state_write_closed)
			{
				return m_io_service.post(
					boost::asio::detail::bind_handler(
						handler,
						boost::asio::error::make_error_code(boost::asio::error::eof),
						0
					)
				);
			}

			m_read_buffer = buffers;
			m_current_read_handler = handler;
			return;
		}

		std::size_t bytes_readed = boost::asio::buffer_copy(boost::asio::buffer(buffers), m_buffer.data());
		m_buffer.consume(bytes_readed);

		m_io_service.post(
			boost::asio::detail::bind_handler(
				handler,
				boost::system::error_code(),
				bytes_readed
			)
		);

		// 接着唤醒 write
		if (m_current_write_handler)
		{
			if (m_buffer.size() <= 512)
			{
				// 唤醒协程.
				m_io_service.post(
					boost::asio::detail::bind_handler(
						m_current_write_handler,
						boost::system::error_code(),
						m_writed_size
					)
				);

				m_current_write_handler = NULL;
			}
		}

	}

	// 关闭, 这样 read 才会读到 eof!
	void shutdown(boost::asio::socket_base::shutdown_type type)
	{
		boost::mutex::scoped_lock l(m_mutex);

		if (type == boost::asio::socket_base::shutdown_send
			|| type == boost::asio::socket_base::shutdown_both)
		{
			// 关闭写
			m_state_write_closed = true;
		}

		if (type == boost::asio::socket_base::shutdown_receive
			|| type == boost::asio::socket_base::shutdown_both)
		{
			// 关闭读
			m_state_read_closed = true;
		}

		if (m_state_write_closed)
		{
			// 检查是否有 read,  是就返回
			if (m_current_read_handler)
			{
				m_io_service.post(
					boost::asio::detail::bind_handler(
						m_current_read_handler,
						boost::asio::error::make_error_code(boost::asio::error::eof),
						0
					)
				);
				m_current_read_handler = NULL;
			}
		}

		if (m_state_read_closed)
		{
			// 检查是否有 read,  是就返回
			if (m_current_write_handler)
			{
				m_io_service.post(
					boost::asio::detail::bind_handler(
						m_current_write_handler,
						boost::asio::error::make_error_code(boost::asio::error::broken_pipe),
						0
					)
				);
				m_current_write_handler = NULL;
			}
		}
	}

	// 写入数据
	template <typename ConstBufferSequence>
	std::size_t write_some(const ConstBufferSequence& buffers)
	{
		// 唤醒正在睡眠的协程.

	}

	// 读取数据
	template <typename ConstBufferSequence>
	std::size_t read_some(const ConstBufferSequence& buffers)
	{
		// 唤醒正在睡眠的协程.

	}

	boost::asio::io_service & get_io_service()
	{
		return m_io_service;
	}

private:
	boost::asio::io_service & m_io_service;

	boost::asio::streambuf m_buffer;

	boost::asio::mutable_buffer m_read_buffer;

	typedef boost::function<
		void (boost::system::error_code, std::size_t)
	> async_handler_type;

	// 注意,  这里意味着同时发起多次 async_read/write 操作, 是不支持的.
	async_handler_type m_current_read_handler;
	async_handler_type m_current_write_handler;

	std::size_t m_writed_size;

	mutable boost::mutex m_mutex;

	bool m_state_write_closed;
	bool m_state_read_closed;
};

} // namespace avhttp

#endif // AVHTTP_INTER_THREAD_STREAM_HPP

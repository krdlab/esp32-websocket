module Lib ( main ) where

import           Control.Monad         (forever)
import qualified Data.ByteString.Char8 as BC
import qualified Data.ByteString.Lazy  as LB
import qualified Data.Text.Encoding    as T
import qualified Data.Text.IO          as T
import qualified Network.WebSockets    as WS
import           Numeric               (showHex)

main :: IO ()
main = WS.runServer "0.0.0.0" 3000 app

app :: WS.ServerApp
app pending = do
    conn <- WS.acceptRequest pending
    WS.forkPingThread conn 10
    echo conn

echo :: WS.Connection -> IO ()
echo conn = do
    putStrLn "start echo"
    forever $ do
        dm <- WS.receiveDataMessage conn
        case dm of
            WS.Text bs _ -> handleTextData conn bs
            WS.Binary bs -> handleBinaryData conn bs
  where
    handleTextData conn bs = do
        let txt = T.decodeUtf8 . LB.toStrict $ bs
        T.putStrLn $ "< (Text) " <> txt
        WS.sendTextData conn bs

    handleBinaryData conn bs = do
        let sbs = BC.pack $ toHexString bs
        BC.putStrLn $ "< (Binary) " <> sbs
        WS.sendBinaryData conn bs

    toHexString =  LB.foldr (\b a -> toHex b <> " " <> a) ""

    toHex x = padding hex (length hex)
      where
        hex = showHex x ""
        padding s 1 = "0" <> s
        padding s _ = s
